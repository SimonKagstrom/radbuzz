#include "buzz_handler.hh"

constexpr auto kAtDistance = 10;

namespace
{

auto
DistanceStringToValue(std::string_view distance)
{
    // TODO: This will not work reliably for non-meter stuff
    if (distance.empty())
    {
        return 0;
    }

    return std::stoi(std::string(distance));
}

} // namespace

BuzzHandler::BuzzHandler(hal::IGpio& left_buzzer,
                         hal::IGpio& right_buzzer,
                         ApplicationState& app_state)
    : m_left_buzzer(left_buzzer)
    , m_right_buzzer(right_buzzer)
    , m_state(app_state)
    , m_state_listener(
          m_state.AttachListener<AS::turn_symbol, AS::distance_to_next>(GetSemaphore()))
    , m_off_timer(StartTimer(0ms))
{
}

std::optional<milliseconds>
BuzzHandler::OnActivation()
{
    auto app_state = m_state.CheckoutReadonly();

    RunStateMachine(app_state);

    m_current_turn = app_state.Get<AS::turn_symbol>();

    return std::nullopt;
}

void
BuzzHandler::RunStateMachine(const ApplicationState::ReadOnly& app_state)
{
    auto before = m_current_state;

    auto ro = m_state.CheckoutReadonly();
    do
    {
        before = m_current_state;

        switch (m_current_state)
        {
        case State::kNoNavigation:
            if (ro.Get<AS::turn_symbol>() != m_current_turn)
            {
                EnterState(State::kNewTurn);
            }
            break;

        case State::kNewTurn:
            EnterState(State::kFar);
            break;
        case State::kFar:
            [[fallthrough]];
        case State::kNear:
            [[fallthrough]];
        case State::kImminent: {
            auto s = DistanceToState(*ro.Get<AS::distance_to_next>());
            if (s != m_current_state)
            {
                EnterState(s);
            }
        }
        break;
        case State::kAt:
            if (DistanceStringToValue(*ro.Get<AS::distance_to_next>()) > kAtDistance)
            {
                EnterState(State::kNewTurn);
            }
            break;

        case State::kValueCount:
            break;
        }
    } while (before != m_current_state);
}


void
BuzzHandler::EnterState(State s)
{
    printf("ENTERING STATE %d\n", (int)s);
    m_current_state = s;

    switch (s)
    {
    case State::kNoNavigation:
        [[fallthrough]];
    case State::kNewTurn:
        [[fallthrough]];
    case State::kFar:
        break;

    case State::kNear:
        [[fallthrough]];
    case State::kImminent:
        [[fallthrough]];
    case State::kAt:
        Indicate();
        break;

    case State::kValueCount:
        break;
    }
}

BuzzHandler::State
BuzzHandler::DistanceToState(std::string_view distance) const
{
    auto distance_int = DistanceStringToValue(distance);

    if (distance_int <= kAtDistance)
    {
        return State::kAt;
    }
    if (distance_int < 20)
    {
        return State::kImminent;
    }
    if (distance_int < 100)
    {
        return State::kNear;
    }

    return State::kFar;
}

void
BuzzHandler::Indicate()
{
    // Make sure it's turned off, if we have an early exit from the last indication
    m_left_buzzer.SetState(false);
    m_right_buzzer.SetState(false);


    auto delay = 100ms;

    switch (m_state.CheckoutReadonly().Get<AS::turn_symbol>())
    {
    case TurnSymbol::kTurnLeft:
    case TurnSymbol::kRampLeft:
    case TurnSymbol::kForkLeft:
    case TurnSymbol::kTurnSharpLeft:
    case TurnSymbol::kUturnLeft:
    case TurnSymbol::kRoundaboutLeft:
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(false);
        break;
    case TurnSymbol::kTurnRight:
    case TurnSymbol::kRampRight:
    case TurnSymbol::kForkRight:
    case TurnSymbol::kTurnSharpRight:
    case TurnSymbol::kUturnRight:
    case TurnSymbol::kRoundaboutRight:
        m_left_buzzer.SetState(false);
        m_right_buzzer.SetState(true);
        delay = 200ms;
        break;
    case TurnSymbol::kStraight:
    case TurnSymbol::kMerge:
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(true);
        break;
    case TurnSymbol::kDestination:
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(true);
        delay = 400ms;
        break;
    default:
        break;
    }

    m_off_timer = StartTimer(delay, [this]() {
        m_left_buzzer.SetState(false);
        m_right_buzzer.SetState(false);
        return std::nullopt;
    });
}
