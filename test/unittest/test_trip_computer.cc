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

            THEN("is_moving is cleared after 5s")
            {
                AdvanceTime(1ms);
                DoRunLoop();
                REQUIRE(rw.Get<AS::is_moving>() == false);
            }
        }
    }
}

TEST_SUITE_END();
