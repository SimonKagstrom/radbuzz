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
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().recent_power_distance = 50;

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
    auto rw = state.CheckoutReadWrite();
    rw.Set<AS::can_bus_active>(true);

    AdvanceTimeAndRunLoop(1s);
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
    rw.Set<AS::can_bus_active>(true);
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
            rw.Post<AS::reset_trip>();
            DoRunLoop();

            THEN("the trip duration is reset")
            {
                REQUIRE(rw.Get<AS::trip_duration>() == 0s);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "trip_distance and trip_average_speed is set by the trip computer")
{
    auto rw = state.CheckoutReadWrite();
    rw.Set<AS::can_bus_active>(true);
    rw.Set<AS::odometer>(1000);

    AdvanceTimeAndRunLoop(1s);
    REQUIRE(rw.Get<AS::is_moving>() == false);
    REQUIRE(rw.Get<AS::trip_distance>() == 0);

    WHEN("the odometer is updated")
    {
        // 2m/s -> ~7km/h
        for (auto i = 0; i < 60; i++)
        {
            rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 2);
            AdvanceTimeAndRunLoop(1s);
        }

        THEN("trip_distance is set")
        {
            REQUIRE(rw.Get<AS::trip_distance>() == 2 * 60);
        }
        AND_THEN("the average speed is calculated")
        {
            REQUIRE(rw.Get<AS::trip_average_speed>() == 7);
        }

        WHEN("the moped is no longer moving")
        {
            AdvanceTimeAndRunLoop(1min);
            REQUIRE(rw.Get<AS::is_moving>() == false);

            THEN("the average is no longer updated")
            {
                REQUIRE(rw.Get<AS::trip_average_speed>() >= 6);
                REQUIRE(rw.Get<AS::trip_average_speed>() <= 7);
            }

            AND_WHEN("the moped starts moving again")
            {
                // 4m/s for one minute
                for (auto i = 0; i < 60; i++)
                {
                    rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 4);
                    AdvanceTimeAndRunLoop(1s);
                }

                THEN("the average speed is updated again")
                {
                    // 2m/s for 1 minute, then 4m/s for 1 minute -> average speed = 3m/s -> 10.8km/h
                    REQUIRE(rw.Get<AS::trip_average_speed>() == 10);
                }
            }
        }

        WHEN("the trip is reset")
        {
            rw.Post<AS::reset_trip>();
            DoRunLoop();

            THEN("the trip speed and distance are reset")
            {
                REQUIRE(rw.Get<AS::trip_distance>() == 0);
                REQUIRE(rw.Get<AS::trip_average_speed>() == 0);
            }

            AND_WHEN("the moped has moved a bit")
            {
                // 4m/s for one minute
                for (auto i = 0; i < 60; i++)
                {
                    rw.Set<AS::odometer>(rw.Get<AS::odometer>() + 4);
                    AdvanceTimeAndRunLoop(1s);
                }

                THEN("the average speed and distance are updated again")
                {
                    // 4m/s for 1 minute -> average speed = 4m/s -> 14.4km/h
                    REQUIRE(rw.Get<AS::trip_average_speed>() == 14);
                    REQUIRE(rw.Get<AS::trip_distance>() == 4 * 60);
                }
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "trip histograms are zeroed by default")
{
    auto histogram = trip_computer.GetRecentEntries();

    REQUIRE(histogram.size() == 10);
    for (const auto& entry : histogram)
    {
        REQUIRE(entry.power == 0);
        REQUIRE(entry.average_consumption == 0);
    }
}

TEST_CASE_FIXTURE(Fixture,
                  "the last entry of the trip histogram is updated with the current average")
{
    auto rw = state.CheckoutReadWrite();
    auto conf = rw.Get<AS::configuration>();

    rw.Set<AS::can_bus_active>(true);
    rw.Set<AS::odometer>(0);
    // Startup
    AdvanceTimeAndRunLoop(100ms);

    WHEN("one sample has been gotten")
    {
        rw.Set<AS::current_power_w>(100);
        rw.Set<AS::wh_consumed>(100);
        rw.Set<AS::wh_regenerated>(50);
        rw.Set<AS::odometer>(1000);
        AdvanceTimeAndRunLoop(250ms);

        THEN("the last entry of the histogram is updated")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.power == 100);
            REQUIRE(last_entry.average_consumption == 50);
        }
    }


    WHEN("two samples have been gotten")
    {
        rw.Set<AS::current_power_w>(100);
        rw.Set<AS::wh_consumed>(100);
        AdvanceTimeAndRunLoop(250ms);

        // Instant
        rw.Set<AS::current_power_w>(200);
        // Accumulating
        rw.Set<AS::wh_consumed>(200);
        AdvanceTimeAndRunLoop(250ms);

        THEN("the last entry of the histogram is updated with the average")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.power == 150);
            REQUIRE(last_entry.average_consumption == 100);
        }

        AND_WHEN("the moped moves")
        {
            rw.Set<AS::odometer>(conf->recent_power_distance + 1);
            rw.Set<AS::current_power_w>(300);
            rw.Set<AS::wh_consumed>(400);
            AdvanceTimeAndRunLoop(250ms);

            auto histogram_entries = trip_computer.GetRecentEntries();
            auto& last_entry = histogram_entries[histogram_entries.size() - 1];
            auto& second_last_entry = histogram_entries[histogram_entries.size() - 2];

            THEN("the current entry is pushed and the last entry of the histogram is updated with "
                 "the new values")
            {
                REQUIRE(second_last_entry.power == 150);
                REQUIRE(second_last_entry.average_consumption == 100);
                REQUIRE(last_entry.power == 300);
            }
        }
    }
}

TEST_SUITE_END();
