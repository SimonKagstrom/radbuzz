#include "buzz_handler.hh"

constexpr auto kAtDistance = 10;

BuzzHandler::BuzzHandler(hal::IGpio& left_buzzer,
                         hal::IGpio& right_buzzer,
                         ApplicationState& app_state)
    : m_left_buzzer(left_buzzer)
    , m_right_buzzer(right_buzzer)
    , m_state(app_state)
    , m_state_listener(m_state.AttachListener(GetSemaphore()))
    , m_off_timer(StartTimer(0ms))
{
}

std::optional<milliseconds>
BuzzHandler::OnActivation()
{
    auto app_state = m_state.CheckoutReadonly();

    RunStateMachine(app_state);

    m_current_hash = app_state->current_icon_hash;

    return std::nullopt;
}

void
BuzzHandler::RunStateMachine(const ApplicationState::State* app_state)
{
    auto before = m_current_state;

    do
    {
        before = m_current_state;

        switch (m_current_state)
        {
        case State::kNoNavigation:
            if (app_state->current_icon_hash != m_current_hash)
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
            auto s = DistanceToState(app_state->distance_to_next);
            if (s != m_current_state)
            {
                EnterState(s);
            }
        }
        break;
        case State::kAt:
            if (app_state->distance_to_next > kAtDistance)
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
BuzzHandler::DistanceToState(uint32_t distance) const
{
    if (distance <= kAtDistance)
    {
        return State::kAt;
    }
    if (distance < 20)
    {
        return State::kImminent;
    }
    if (distance < 100)
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

    switch (m_state.CheckoutReadonly()->current_icon_hash)
    {
    case 0x27d9a40f: // Left
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(false);
        break;
    case 0x09cefe42: // Route target + road
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(true);
        delay = 200ms;
        break;
    case 0x3db5d6ba:
        [[fallthrough]];
    case 0x28936c85: // Roundabout straight ahead
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(true);
        break;
    case 0x9a84a956: // Destination reached
        m_left_buzzer.SetState(true);
        m_right_buzzer.SetState(true);
        delay = 400ms;
    default:
        break;
    }
    printf("Buzzing for %d ms\n", delay.count());

    m_off_timer = StartTimer(delay, [this]() {
        printf("BUZZ OFF!\n");
        m_left_buzzer.SetState(false);
        m_right_buzzer.SetState(false);
        return std::nullopt;
    });
}
