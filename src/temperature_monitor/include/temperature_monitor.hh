#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "pooled_thread_base.hh"

class TemperatureMonitor : public PooledThreadBase
{
public:
    explicit TemperatureMonitor(ApplicationState& state);

    std::optional<milliseconds> OnActivation() final;

private:
    bool IsOverheated() const;

ApplicationState& m_state;
    std::unique_ptr<ListenerCookie> m_state_listener;
    os::TimerHandle m_overheated_timer;
};
