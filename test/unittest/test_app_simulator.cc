#include "app_simulator.hh"
#include "job_pool_thread.hh"
#include "test.hh"
#include "thread_fixture.hh"

namespace
{

constexpr uint32_t kUtc = 1774269319;

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

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;

    JobPoolThread job_pool;
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
        CHECK(state.Get<AS::gps_utc>() == AppSimulator::kDemoStartUtc + 1);
    }

    AND_WHEN("time passes")
    {
        // A step is at most 200 ms
        AdvanceTimeAndRunLoop(1s);

        THEN("a second passes for each step")
        {
            CHECK(state.Get<AS::gps_utc>() >= AppSimulator::kDemoStartUtc + 1 + 5);
        }

        AND_WHEN("demo mode is restarted")
        {
            const auto before = state.Get<AS::gps_utc>();

            SetDemoMode(false);
            SetDemoMode(true);

            THEN("the time continues, so that demo trips get new times")
            {
                CHECK(state.Get<AS::gps_utc>() == before + 1);
            }
        }
    }
}

TEST_SUITE_END();
