#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_can.hh"

class CanBusHandler : public os::BaseThread
{
public:
    CanBusHandler(hal::ICan& bus, ApplicationState& app_state, uint8_t controller_id);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void
    VescResponseCallback(uint8_t controller_id, uint8_t command, const uint8_t* data, uint8_t len);

    hal::ICan& m_bus;

    ApplicationState& m_state;
    std::unique_ptr<ApplicationState::IListener> m_state_listener;
    std::unique_ptr<ListenerCookie> m_bus_listener;
};
