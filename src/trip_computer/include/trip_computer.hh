#pragma once

#include "application_state.hh"
#include "base_thread.hh"

#include <etl/circular_buffer.h>

class TripComputer : public os::BaseThread
{
public:
    explicit TripComputer(ApplicationState& app_state);

private:
    std::optional<milliseconds> OnActivation() final;

    void UpdateSoc(uint16_t millivolts);

    ApplicationState& m_state;

    std::unique_ptr<ListenerCookie> m_state_listener;
    os::TimerHandle m_soc_timer;

    etl::circular_buffer<uint16_t, 10> m_millivolt_history;
};
