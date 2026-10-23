#include "buzz_handler.hh"
#include "hal/mock/mock_gpio.hh"
#include "test.hh"
#include "thread_fixture.hh"

namespace
{

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(&buzz);

        buzz.Start("buzz_handler");
        RunLoopWhileReady();
    }

    void Update(TurnSymbol symbol, uint32_t distance)
    {
        state.Set<AS::turn_symbol>(symbol);
        state.Set<AS::distance_to_next>(distance);

        // Let the buzzers turn off before the next update
        AdvanceTimeAndRunLoop(1s);
    }

    ApplicationState state;
    MockGpio left;
    MockGpio right;

    int left_buzzes {0};
    int right_buzzes {0};

    // Before buzz, since the buzzers are turned off in the constructor
    std::unique_ptr<trompeloeil::expectation> r_left_off {
        NAMED_ALLOW_CALL(left, SetState(false))};
    std::unique_ptr<trompeloeil::expectation> r_right_off {
        NAMED_ALLOW_CALL(right, SetState(false))};
    std::unique_ptr<trompeloeil::expectation> r_left_on {
        NAMED_ALLOW_CALL(left, SetState(true)).LR_SIDE_EFFECT(++left_buzzes)};
    std::unique_ptr<trompeloeil::expectation> r_right_on {
        NAMED_ALLOW_CALL(right, SetState(true)).LR_SIDE_EFFECT(++right_buzzes)};

    BuzzHandler buzz {left, right, state};
};

using State = BuzzHandler::State;

} // namespace


TEST_SUITE_BEGIN("buzz_handler");

TEST_CASE_FIXTURE(Fixture, "the buzz handler is idle without navigation")
{
    REQUIRE(buzz.CurrentState() == State::kNoNavigation);

    Update(TurnSymbol::kNone, 0);
    REQUIRE(buzz.CurrentState() == State::kNoNavigation);
    REQUIRE(left_buzzes == 0);
    REQUIRE(right_buzzes == 0);
}

TEST_CASE_FIXTURE(Fixture, "the turn is indicated when approaching it")
{
    Update(TurnSymbol::kTurnLeft, 300);
    REQUIRE(buzz.CurrentState() == State::kFar);
    REQUIRE(left_buzzes == 0);

    Update(TurnSymbol::kTurnLeft, 200);
    REQUIRE(left_buzzes == 0);

    Update(TurnSymbol::kTurnLeft, 90);
    REQUIRE(buzz.CurrentState() == State::kNear);
    REQUIRE(left_buzzes == 1);

    Update(TurnSymbol::kTurnLeft, 50);
    REQUIRE(left_buzzes == 1);

    Update(TurnSymbol::kTurnLeft, 15);
    REQUIRE(buzz.CurrentState() == State::kImminent);
    REQUIRE(left_buzzes == 2);

    Update(TurnSymbol::kTurnLeft, 5);
    REQUIRE(buzz.CurrentState() == State::kAt);
    REQUIRE(left_buzzes == 3);

    REQUIRE(right_buzzes == 0);

    WHEN("the turn is passed, and the next is shown")
    {
        Update(TurnSymbol::kTurnRight, 400);

        THEN("it's a new turn, which is indicated when approached")
        {
            REQUIRE(buzz.CurrentState() == State::kFar);
            REQUIRE(right_buzzes == 0);

            Update(TurnSymbol::kTurnRight, 80);
            REQUIRE(right_buzzes == 1);
            REQUIRE(left_buzzes == 3);
        }
    }

    WHEN("the next turn has the same symbol")
    {
        Update(TurnSymbol::kTurnLeft, 400);

        THEN("the distance jump makes it a new turn")
        {
            REQUIRE(buzz.CurrentState() == State::kFar);

            Update(TurnSymbol::kTurnLeft, 80);
            REQUIRE(left_buzzes == 4);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a new turn which is already near is indicated directly")
{
    Update(TurnSymbol::kTurnRight, 50);

    REQUIRE(buzz.CurrentState() == State::kNear);
    REQUIRE(right_buzzes == 1);
    REQUIRE(left_buzzes == 0);
}

TEST_CASE_FIXTURE(Fixture, "GPS jitter doesn't cause repeated indications")
{
    Update(TurnSymbol::kTurnLeft, 300);
    Update(TurnSymbol::kTurnLeft, 90);
    REQUIRE(left_buzzes == 1);

    for (auto d : {95, 85, 97, 88, 99, 80})
    {
        Update(TurnSymbol::kTurnLeft, d);
    }

    REQUIRE(buzz.CurrentState() == State::kNear);
    REQUIRE(left_buzzes == 1);
}

TEST_CASE_FIXTURE(Fixture, "moving away from the turn is not indicated")
{
    Update(TurnSymbol::kTurnLeft, 300);
    Update(TurnSymbol::kTurnLeft, 60);
    REQUIRE(left_buzzes == 1);

    Update(TurnSymbol::kTurnLeft, 80);
    REQUIRE(buzz.CurrentState() == State::kReceding);

    Update(TurnSymbol::kTurnLeft, 100);
    Update(TurnSymbol::kTurnLeft, 110);
    REQUIRE(buzz.CurrentState() == State::kReceding);
    REQUIRE(left_buzzes == 1);

    WHEN("turning back towards the turn")
    {
        Update(TurnSymbol::kTurnLeft, 90);

        THEN("it's indicated again")
        {
            REQUIRE(buzz.CurrentState() == State::kNear);
            REQUIRE(left_buzzes == 2);
        }
    }

    WHEN("continuing in the wrong direction")
    {
        Update(TurnSymbol::kTurnLeft, 130);

        THEN("the turn is far again, and indicated when approached")
        {
            REQUIRE(buzz.CurrentState() == State::kFar);
            REQUIRE(left_buzzes == 1);

            Update(TurnSymbol::kTurnLeft, 90);
            REQUIRE(left_buzzes == 2);
        }
    }

    WHEN("the navigation app reroutes")
    {
        Update(TurnSymbol::kTurnRight, 150);

        THEN("the new turn is used")
        {
            REQUIRE(buzz.CurrentState() == State::kFar);

            Update(TurnSymbol::kTurnRight, 70);
            REQUIRE(right_buzzes == 1);
            REQUIRE(left_buzzes == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "overshooting the turn is not indicated")
{
    Update(TurnSymbol::kTurnLeft, 300);
    Update(TurnSymbol::kTurnLeft, 5);
    REQUIRE(buzz.CurrentState() == State::kAt);
    REQUIRE(left_buzzes == 1);

    Update(TurnSymbol::kTurnLeft, 25);
    REQUIRE(buzz.CurrentState() == State::kReceding);
    REQUIRE(left_buzzes == 1);
}

TEST_CASE_FIXTURE(Fixture, "navigation can end in any state")
{
    Update(TurnSymbol::kTurnLeft, 300);
    Update(TurnSymbol::kTurnLeft, 50);
    REQUIRE(buzz.CurrentState() == State::kNear);

    WHEN("the turn symbol is cleared")
    {
        Update(TurnSymbol::kNone, 50);
        REQUIRE(buzz.CurrentState() == State::kNoNavigation);
    }

    WHEN("the distance is cleared")
    {
        Update(TurnSymbol::kTurnLeft, 0);
        REQUIRE(buzz.CurrentState() == State::kNoNavigation);
    }

    AND_THEN("restarting the navigation indicates the turn again")
    {
        Update(TurnSymbol::kNone, 0);
        Update(TurnSymbol::kTurnLeft, 50);

        REQUIRE(buzz.CurrentState() == State::kNear);
        REQUIRE(left_buzzes == 2);
    }
}
