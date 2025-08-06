#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_gpio.hh"

class BuzzHandler : public os::BaseThread
{
public:
    BuzzHandler(hal::IGpio& left_buzzer, hal::IGpio& right_buzzer, ApplicationState& app_state);

private:
    std::optional<milliseconds> OnActivation() final;

    hal::IGpio& m_left_buzzer;
    hal::IGpio& m_right_buzzer;

    ApplicationState& m_state;


    std::unique_ptr<ApplicationState::IListener> m_state_listener;
};
