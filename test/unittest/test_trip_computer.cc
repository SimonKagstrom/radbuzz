#include "job_pool_thread.hh"
#include "test.hh"
#include "thread_fixture.hh"
#include "trip_computer.hh"

#include <cmath>


namespace
{

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().recent_power_distance = 50;

        // Pooled, like on the target. The job pool owns it from here on
        job_pool.AttachPooledThread(std::move(trip_computer_owner));
        SetThread(&job_pool);

        job_pool.Start("job_pool");
    }


    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;

    std::unique_ptr<TripComputer> trip_computer_owner {
        std::make_unique<TripComputer>(state, post_office)};
    TripComputer& trip_computer {*trip_computer_owner};

    JobPoolThread job_pool;
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
            post_office.Send<MSG::reset_trip>();
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
            post_office.Send<MSG::reset_trip>();

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
        // 2Wh net over 40m (within the same bucket) -> 50Wh/km
        rw.Set<AS::current_power_w>(100);
        rw.Set<AS::wh_consumed>(4);
        rw.Set<AS::wh_regenerated>(2);
        rw.Set<AS::odometer>(40);
        AdvanceTimeAndRunLoop(250ms);

        THEN("the last entry of the histogram is updated")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.power == 100);
            REQUIRE(last_entry.average_consumption == 50);
        }
    }

    WHEN("one negative power value has been gotten (due to regen)")
    {
        rw.Set<AS::current_power_w>(-100);
        rw.Set<AS::odometer>(1000);

        AdvanceTimeAndRunLoop(250ms);

        THEN("the value in the histogram is capped to zero")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.power == 0);
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
            REQUIRE(last_entry.average_consumption == 0);
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
                REQUIRE(second_last_entry.average_consumption == 0);
                REQUIRE(last_entry.power == 300);
            }
        }
    }

    WHEN("multiple negative samples are gotten")
    {
        rw.Set<AS::odometer>(conf->recent_power_distance + 1);
        rw.Set<AS::current_power_w>(-300);

        AdvanceTimeAndRunLoop(250ms);

        rw.Set<AS::odometer>(conf->recent_power_distance + 1);
        rw.Set<AS::current_power_w>(-300);
        AdvanceTimeAndRunLoop(250ms);

        THEN("the power is still 0")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.power == 0);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the trip histogram consumption is valid when standing still")
{
    auto rw = state.CheckoutReadWrite();

    rw.Set<AS::can_bus_active>(true);
    // Startup
    AdvanceTimeAndRunLoop(100ms);

    WHEN("the moped stands still without consuming anything")
    {
        AdvanceTimeAndRunLoop(1s);

        THEN("the consumption is zero (and not NaN)")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE_FALSE(std::isnan(last_entry.average_consumption));
            REQUIRE(last_entry.average_consumption == 0);
        }
    }
}

TEST_CASE_FIXTURE(Fixture,
                  "the first trip histogram entry is relative to the odometer and consumption at start")
{
    auto rw = state.CheckoutReadWrite();

    // A realistic setup, with previous distance and consumption
    rw.Set<AS::odometer>(10000);
    rw.Set<AS::wh_consumed>(200);
    rw.Set<AS::can_bus_active>(true);
    // Startup
    AdvanceTimeAndRunLoop(100ms);
    AdvanceTimeAndRunLoop(250ms);

    WHEN("the moped moves 10m (in the same bucket) and consumes 0.5Wh")
    {
        rw.Set<AS::odometer>(10010);
        rw.Set<AS::wh_consumed>(200.5f);
        AdvanceTimeAndRunLoop(250ms);

        THEN("the consumption is 50Wh/km")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.average_consumption == doctest::Approx(50));
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a new trip histogram entry starts out empty")
{
    auto rw = state.CheckoutReadWrite();
    auto conf = rw.Get<AS::configuration>();

    rw.Set<AS::odometer>(1000);
    rw.Set<AS::wh_consumed>(10);
    rw.Set<AS::can_bus_active>(true);
    // Startup
    AdvanceTimeAndRunLoop(100ms);
    AdvanceTimeAndRunLoop(250ms);

    rw.Set<AS::odometer>(1010);
    rw.Set<AS::wh_consumed>(10.5f);
    AdvanceTimeAndRunLoop(250ms);

    WHEN("the moped moves into the next bucket")
    {
        rw.Set<AS::odometer>(1000 + conf->recent_power_distance + 10);
        rw.Set<AS::wh_consumed>(13);
        AdvanceTimeAndRunLoop(250ms);

        THEN("the new entry has no consumption yet (i.e., not the Wh of the previous bucket)")
        {
            auto& last_entry = trip_computer.GetRecentEntries().back();

            REQUIRE(last_entry.average_consumption == 0);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "movement within a histogram bucket is detected, but doesn't push entries")
{
    auto rw = state.CheckoutReadWrite();
    auto conf = rw.Get<AS::configuration>();

    rw.Set<AS::odometer>(1000);
    rw.Set<AS::can_bus_active>(true);
    // Startup
    AdvanceTimeAndRunLoop(100ms);

    rw.Set<AS::current_power_w>(100);

    WHEN("the moped moves slowly, within a bucket")
    {
        for (auto i = 1; i < conf->recent_power_distance; i++)
        {
            rw.Set<AS::odometer>(1000 + i);
            AdvanceTimeAndRunLoop(250ms);

            // Movement is detected on each odometer update
            REQUIRE(rw.Get<AS::is_moving>());
        }

        THEN("no new histogram entry is pushed")
        {
            auto histogram_entries = trip_computer.GetRecentEntries();
            auto& last_entry = histogram_entries[histogram_entries.size() - 1];
            auto& second_last_entry = histogram_entries[histogram_entries.size() - 2];

            REQUIRE(last_entry.power == 100);
            REQUIRE(second_last_entry.power == 0);
        }

        AND_WHEN("the moped crosses into the next bucket")
        {
            rw.Set<AS::odometer>(1000 + conf->recent_power_distance);
            AdvanceTimeAndRunLoop(250ms);

            THEN("exactly one entry is pushed")
            {
                auto histogram_entries = trip_computer.GetRecentEntries();
                auto& second_last_entry = histogram_entries[histogram_entries.size() - 2];
                auto& third_last_entry = histogram_entries[histogram_entries.size() - 3];

                REQUIRE(rw.Get<AS::is_moving>());
                REQUIRE(second_last_entry.power == 100);
                REQUIRE(third_last_entry.power == 0);
            }
        }
    }
}

TEST_SUITE_END();
