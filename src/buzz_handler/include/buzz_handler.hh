#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "buzz_handler_state_machine_base.hh"
#include "hal/i_gpio.hh"

#include <limits>

struct BuzzHandlerStateData : BuzzHandlerStateMachineData
{
    // The closest distance seen in the state, to detect moving away
    struct Closest
    {
        uint32_t closest {std::numeric_limits<uint32_t>::max()};
    };

    struct Near : BuzzHandlerStateMachineData::Near, Closest
    {
    };

    struct Imminent : BuzzHandlerStateMachineData::Imminent, Closest
    {
    };

    struct At : BuzzHandlerStateMachineData::At, Closest
    {
    };

    struct Receding : BuzzHandlerStateMachineData::Receding
    {
        // The farthest distance seen, to detect moving closer again
        uint32_t farthest {0};
    };
};

class BuzzHandler
    : public os::BaseThread
    , public BuzzHandlerStateMachineBase<BuzzHandlerStateData>
{
public:
    BuzzHandler(hal::IGpio& left_buzzer, hal::IGpio& right_buzzer, ApplicationState& app_state);

private:
    NoNavigation::Next Evaluate(NoNavigation&) final;

    void Enter(NewTurn&) final;
    NewTurn::Next Evaluate(NewTurn&) final;

    Far::Next Evaluate(Far&) final;

    void Enter(Near&) final;
    Near::Next Evaluate(Near&) final;

    void Enter(Imminent&) final;
    Imminent::Next Evaluate(Imminent&) final;

    void Enter(At&) final;
    At::Next Evaluate(At&) final;

    Receding::Next Evaluate(Receding&) final;

    void OnTransition(State from, State to) final;

    std::optional<milliseconds> OnActivation() final;

    bool NavigationActive() const;

    bool TurnChanged() const;

    void Indicate();

    hal::IGpio& m_left_buzzer;
    hal::IGpio& m_right_buzzer;

    ApplicationState& m_state;


    ListenerCookie m_state_listener;

    // The turn being indicated, and the distance at the last activation
    TurnSymbol m_current_turn {TurnSymbol::kNone};
    uint32_t m_last_distance {0};

    os::TimerHandle m_off_timer;
};
