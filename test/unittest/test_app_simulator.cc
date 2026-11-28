#include "app_simulator.hh"
#include "job_pool_thread.hh"
#include "mock_event_notifier.hh"
#include "test.hh"
#include "thread_fixture.hh"

namespace
{

constexpr uint32_t kUtc = 1774269319;
constexpr uint32_t kDemoStartUtc = 1438365600;
constexpr uint32_t kStep = 13 * 60 * 60;

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        // Pooled, like on the target. The job pool owns it from here on
        job_pool.AttachPooledThread(std::make_unique<AppSimulator>(state, post_office));
        SetThread(&job_pool);

        job_pool.Start("job_pool");

        // Started before demo mode is changed, as on the target
        RunLoopWhileReady();
    }

    void SetDemoMode(bool active)
    {
        state.Set<AS::demo_mode>(active);
        RunLoopWhileReady();
    }

    // Within a second, where the steps are at least 50 ms
    bool IsSoonAfter(uint32_t start_utc)
    {
        const auto utc = state.Get<AS::gps_utc>();

        return utc > start_utc && utc <= start_utc + 21;
    }

    // keep_recorded_trip of the trip resets since the last call
    std::vector<bool> TakeTripResets()
    {
        std::vector<bool> out;
        m_mailbox->Collect().On<MSG::reset_trip>(
            [&out](auto reset) { out.push_back(reset->keep_recorded_trip); });

        return out;
    }

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;

    JobPoolThread job_pool;

private:
    NullEventNotifier m_notifier;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox {
        post_office.Subscribe<MSG::reset_trip>(m_notifier)};
};

} // namespace


TEST_SUITE_BEGIN("app_simulator");

TEST_CASE_FIXTURE(Fixture, "the GPS time isn't set outside of demo mode")
{
    RunLoopWhileReady();
    AdvanceTimeAndRunLoop(1s);

    CHECK(state.Get<AS::gps_utc>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "the demo GPS time runs faster than real time")
{
    // A real GPS time is ignored, since the demo time would race ahead of it anyway
    state.Set<AS::gps_utc>(kUtc);
    SetDemoMode(true);

    THEN("it starts from a fixed time")
    {
        CHECK(IsSoonAfter(kDemoStartUtc));
    }

    AND_WHEN("time passes")
    {
        // A step is at most 200 ms
        AdvanceTimeAndRunLoop(1s);

        THEN("a second passes for each step")
        {
            CHECK(state.Get<AS::gps_utc>() >= kDemoStartUtc + 1 + 5);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the trip is reset on demo mode changes")
{
    SetDemoMode(true);

    THEN("a real trip before it is kept")
    {
        CHECK(TakeTripResets() == std::vector {true});
    }

    AND_WHEN("demo mode is turned off")
    {
        TakeTripResets();
        SetDemoMode(false);

        THEN("the demo trip isn't kept")
        {
            CHECK(TakeTripResets() == std::vector {false});
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the demo GPS time steps forward for each start of demo mode")
{
    WHEN("demo mode is started")
    {
        SetDemoMode(true);

        THEN("the next start is later")
        {
            CHECK(state.Get<AS::demo::next_start_utc>() == kDemoStartUtc + kStep);
        }

        AND_WHEN("the trip is reset")
        {
            AdvanceTimeAndRunLoop(1s);

            const auto before = state.Get<AS::gps_utc>();
            post_office.Send<MSG::reset_trip>();
            AdvanceTimeAndRunLoop(1s);

            THEN("the time just continues, so the next trip also has new times")
            {
                CHECK(state.Get<AS::gps_utc>() > before);
                CHECK(IsSoonAfter(before));
                CHECK(state.Get<AS::demo::next_start_utc>() == kDemoStartUtc + kStep);
            }
        }

        AND_WHEN("demo mode is turned off")
        {
            const auto before = state.Get<AS::gps_utc>();

            SetDemoMode(false);
            AdvanceTimeAndRunLoop(1s);

            THEN("the time stops, without a step")
            {
                CHECK(state.Get<AS::gps_utc>() == before);
                CHECK(state.Get<AS::demo::next_start_utc>() == kDemoStartUtc + kStep);
            }

            AND_WHEN("it's turned on again")
            {
                SetDemoMode(true);

                THEN("the time has stepped once")
                {
                    CHECK(IsSoonAfter(kDemoStartUtc + kStep));
                    CHECK(state.Get<AS::demo::next_start_utc>() == kDemoStartUtc + 2 * kStep);
                }
            }
        }
    }

    WHEN("demo mode has been used in an earlier run")
    {
        // From the NVM
        state.Set<AS::demo::next_start_utc>(kUtc);
        SetDemoMode(true);

        THEN("it starts after that")
        {
            CHECK(IsSoonAfter(kUtc));
            CHECK(state.Get<AS::demo::next_start_utc>() == kUtc + kStep);
        }
    }
}

TEST_SUITE_END();
