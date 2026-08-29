#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_can.hh"

struct VescCanState;

class CanBusHandler : public os::BaseThread
{
public:
    CanBusHandler(hal::ICan& bus, ApplicationState& app_state);
    ~CanBusHandler();

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void SetMaxSpeed(uint8_t max_speed_kmh);

    void
    VescResponseCallback(uint8_t controller_id, uint8_t command, const uint8_t* data, uint8_t len);

    hal::ICan& m_bus;
    ApplicationState& m_state;
    std::optional<uint8_t> m_controller_id;
    ApplicationState::PartialReadOnlyCache<AS::configuration> m_state_cache;
    std::unique_ptr<ListenerCookie> m_state_listener;

    // Unknown, so not unique_ptr
    VescCanState *m_vesc_can_state {nullptr};

    std::unique_ptr<ListenerCookie> m_bus_listener;

    os::TimerHandle m_periodic_timer;
    os::TimerHandle m_start_timer;
    os::TimerHandle m_overheated_timer;

    float m_start_consumed_wh {0.0f};
    float m_start_regen_wh {0.0f};
};
