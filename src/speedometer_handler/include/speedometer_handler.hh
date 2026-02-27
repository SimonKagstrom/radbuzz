#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_stepper_motor.hh"

class SpeedometerHandler : public os::BaseThread
{
public:
    SpeedometerHandler(hal::IStepperMotor& motor,
                       ApplicationState& app_state,
                       int32_t zero_to_max_steps);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    hal::IStepperMotor& m_motor;
    ApplicationState& m_state;
    const int32_t m_zero_to_max_steps;

    std::unique_ptr<ListenerCookie> m_state_listener;

    int32_t m_position {0};
    milliseconds m_last_step_time {0};
};
