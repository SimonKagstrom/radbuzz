#include "test.hh"
#include "thread_fixture.hh"
#include "trip_computer.hh"


namespace
{

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(&trip_computer);

        trip_computer.Start("trip_computer");
    }


    ApplicationState state;
    TripComputer trip_computer {state};
};

} // namespace


TEST_SUITE_BEGIN("trip_computer");

TEST_CASE_FIXTURE(Fixture, "is_moving is setup by the trip computer")
{
    DoRunLoop();
    auto rw = state.CheckoutReadWrite();
    REQUIRE(rw.Get<AS::is_moving>() == false);

    WHEN("the distance is updated")
    {
        AdvanceTime(1s);
        rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 1);
        DoRunLoop();

        THEN("is_moving is set")
        {
            REQUIRE(rw.Get<AS::is_moving>() == true);
        }

        AND_WHEN("the moped stops, and the distance no longer updates")
        {
            AdvanceTime(4999ms);
            DoRunLoop();
            REQUIRE(rw.Get<AS::is_moving>() == true);
            AdvanceTime(1ms);
            DoRunLoop();

            THEN("is_moving is cleared after 5s")
            {
                REQUIRE(rw.Get<AS::is_moving>() == false);
            }

            AND_WHEN("is_moving is set when the moped starts moving again")
            {
                rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 1);
                AdvanceTime(250ms);
                DoRunLoop();

                REQUIRE(rw.Get<AS::is_moving>() == true);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "trip_duration is updated when the moped is moving")
{
    auto rw = state.CheckoutReadWrite();
    // Some misaligned value to mimic the real world
    AdvanceTimeAndRunLoop(17399ms);

    REQUIRE(rw.Get<AS::trip_duration>() == 0s);
    REQUIRE(rw.Get<AS::is_moving>() == false);
    REQUIRE(rw.Get<AS::trip_duration>() == 0s);

    WHEN("the moped starts moving")
    {
        // Triggered every 250ms
        rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 1);
        AdvanceTimeAndRunLoop(250ms);
        REQUIRE(rw.Get<AS::is_moving>());
        REQUIRE(rw.Get<AS::trip_duration>() == 0s);

        // Up to 1s
        AdvanceTimeAndRunLoop(750ms);

        THEN("the trip duration is updated")
        {
            REQUIRE(rw.Get<AS::trip_duration>() == 1s);
        }

        AND_THEN("it's updated every started second")
        {
            AdvanceTimeAndRunLoop(1s);
            REQUIRE(rw.Get<AS::trip_duration>() == 2s);

            AdvanceTimeAndRunLoop(2s);
            REQUIRE(rw.Get<AS::trip_duration>() == 4s);

            AND_THEN("it stops updating when the moped stops moving")
            {
                AdvanceTimeAndRunLoop(10s);
                REQUIRE(rw.Get<AS::trip_duration>() == 5s);

                AND_WHEN("the moped starts moving again")
                {
                    rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 1);

                    AdvanceTimeAndRunLoop(1s);
                    REQUIRE(rw.Get<AS::is_moving>());

                    THEN("counting starts again")
                    {
                        REQUIRE(rw.Get<AS::trip_duration>() == 6s);
                    }
                }
            }
        }

        WHEN("the trip is reset")
        {
            // TBD
        }
    }
}

TEST_SUITE_END();
