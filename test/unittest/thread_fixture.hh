#pragma once

#include "base_thread.hh"
#include "mock_time.hh"
#include "test.hh"

class ThreadFixture : public TimeFixture
{
public:
    void SetThread(os::BaseThread* thread)
    {
        m_thread = thread;
    }

    void DoStartup()
    {
        REQUIRE(m_has_started == false);
        m_has_started = true;

        m_thread->OnStartup();
    }

    std::optional<milliseconds> DoRunLoop()
    {
        assert(m_thread);
        return m_thread->RunLoop();
    }

private:
    bool m_has_started {false};
    os::BaseThread* m_thread {nullptr};
};
