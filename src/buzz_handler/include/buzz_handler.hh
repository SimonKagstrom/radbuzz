#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_gpio.hh"

class BuzzHandler : public os::BaseThread
{
public:
    enum class State
    {
        kNoNavigation,
        kNewTurn,
        kFar,
        kNear,
        kImminent,
        kAt,

        kValueCount,
    };

    BuzzHandler(hal::IGpio& left_buzzer, hal::IGpio& right_buzzer, ApplicationState& app_state);

private:
    std::optional<milliseconds> OnActivation() final;

    void RunStateMachine(const ApplicationState::ReadOnly& app_state);

    void Indicate();

    void EnterState(State s);

    State DistanceToState(uint32_t distance) const;

    hal::IGpio& m_left_buzzer;
    hal::IGpio& m_right_buzzer;

    ApplicationState& m_state;


    std::unique_ptr<ApplicationState::IListener> m_state_listener;

    // State data
    State m_current_state {State::kNoNavigation};
    uint32_t m_current_hash;

    os::TimerHandle m_off_timer;
};
