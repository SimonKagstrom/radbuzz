#include "buzz_handler.hh"

#include <algorithm>

constexpr auto kAtDistance = 10;
constexpr auto kImminentDistance = 20;
constexpr auto kNearDistance = 100;

// Moving away/closer by this much (from the closest/farthest seen) changes direction. Avoids
// GPS jitter causing repeated indications
constexpr auto kDirectionMargin = 15;

// Receding beyond this goes back to Far, i.e., the turn is indicated again when approaching
constexpr auto kRearmDistance = kNearDistance + 20;

// A sudden distance increase this large is a new turn (reroute, or the turn after the last)
constexpr auto kNewTurnDistanceJump = 50;

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
    RunStateMachine();

    m_last_distance = Distance();

    return std::nullopt;
}

bool
BuzzHandler::NavigationActive() const
{
    return m_state.Get<AS::turn_symbol>() != TurnSymbol::kNone &&
           !m_state.Get<AS::distance_to_next>()->empty();
}

bool
BuzzHandler::TurnChanged() const
{
    return m_state.Get<AS::turn_symbol>() != m_current_turn ||
           Distance() > m_last_distance + kNewTurnDistanceJump;
}

int
BuzzHandler::Distance() const
{
    return DistanceStringToValue(*m_state.Get<AS::distance_to_next>());
}

void
BuzzHandler::Indicate()
{
    // Make sure it's turned off, if we have an early exit from the last indication
    m_left_buzzer.SetState(false);
    m_right_buzzer.SetState(false);


    auto delay = 100ms;

    switch (m_state.Get<AS::turn_symbol>())
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


void
BuzzHandler::OnTransition(State from, State to)
{
    printf("Buzz handler: %.*s -> %.*s\n",
           static_cast<int>(StateName(from).size()),
           StateName(from).data(),
           static_cast<int>(StateName(to).size()),
           StateName(to).data());
}

BuzzHandler::NoNavigation::Next
BuzzHandler::Evaluate(NoNavigation&)
{
    if (NavigationActive())
    {
        return State::kNewTurn;
    }

    return kStay;
}

void
BuzzHandler::Enter(NewTurn&)
{
    m_current_turn = m_state.Get<AS::turn_symbol>();
    m_last_distance = Distance();
}

BuzzHandler::NewTurn::Next
BuzzHandler::Evaluate(NewTurn&)
{
    auto distance = Distance();

    if (distance <= kAtDistance)
    {
        return State::kAt;
    }
    if (distance < kImminentDistance)
    {
        return State::kImminent;
    }
    if (distance < kNearDistance)
    {
        return State::kNear;
    }

    return State::kFar;
}

BuzzHandler::Far::Next
BuzzHandler::Evaluate(Far&)
{
    if (!NavigationActive())
    {
        return State::kNoNavigation;
    }
    if (TurnChanged())
    {
        return State::kNewTurn;
    }

    auto distance = Distance();

    if (distance <= kAtDistance)
    {
        return State::kAt;
    }
    if (distance < kImminentDistance)
    {
        return State::kImminent;
    }
    if (distance < kNearDistance)
    {
        return State::kNear;
    }

    return kStay;
}

void
BuzzHandler::Enter(Near&)
{
    Indicate();
}

BuzzHandler::Near::Next
BuzzHandler::Evaluate(Near& near)
{
    if (!NavigationActive())
    {
        return State::kNoNavigation;
    }
    if (TurnChanged())
    {
        return State::kNewTurn;
    }

    auto distance = Distance();
    near.closest = std::min(near.closest, distance);

    if (distance > near.closest + kDirectionMargin)
    {
        return State::kReceding;
    }
    if (distance <= kAtDistance)
    {
        return State::kAt;
    }
    if (distance < kImminentDistance)
    {
        return State::kImminent;
    }

    return kStay;
}


void
BuzzHandler::Enter(Imminent&)
{
    Indicate();
}

BuzzHandler::Imminent::Next
BuzzHandler::Evaluate(Imminent& imminent)
{
    if (!NavigationActive())
    {
        return State::kNoNavigation;
    }
    if (TurnChanged())
    {
        return State::kNewTurn;
    }

    auto distance = Distance();
    imminent.closest = std::min(imminent.closest, distance);

    if (distance > imminent.closest + kDirectionMargin)
    {
        return State::kReceding;
    }
    if (distance <= kAtDistance)
    {
        return State::kAt;
    }

    return kStay;
}


void
BuzzHandler::Enter(At&)
{
    Indicate();
}


BuzzHandler::At::Next
BuzzHandler::Evaluate(At& at)
{
    if (!NavigationActive())
    {
        return State::kNoNavigation;
    }
    if (TurnChanged())
    {
        return State::kNewTurn;
    }

    auto distance = Distance();
    at.closest = std::min(at.closest, distance);

    if (distance > at.closest + kDirectionMargin)
    {
        return State::kReceding;
    }

    return kStay;
}

BuzzHandler::Receding::Next
BuzzHandler::Evaluate(Receding& receding)
{
    if (!NavigationActive())
    {
        return State::kNoNavigation;
    }
    if (TurnChanged())
    {
        return State::kNewTurn;
    }

    auto distance = Distance();
    receding.farthest = std::max(receding.farthest, distance);

    if (distance > kRearmDistance)
    {
        return State::kFar;
    }
    if (distance + kDirectionMargin < receding.farthest)
    {
        // Moving closer again
        if (distance <= kAtDistance)
        {
            return State::kAt;
        }
        if (distance < kImminentDistance)
        {
            return State::kImminent;
        }
        if (distance < kNearDistance)
        {
            return State::kNear;
        }
    }

    return kStay;
}
