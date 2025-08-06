#include "buzz_handler.hh"

BuzzHandler::BuzzHandler(hal::IGpio& left_buzzer,
                         hal::IGpio& right_buzzer,
                         ApplicationState& app_state)
    : m_left_buzzer(left_buzzer)
    , m_right_buzzer(right_buzzer)
    , m_state(app_state)
    , m_state_listener(m_state.AttachListener(GetSemaphore()))
{
}

std::optional<milliseconds>
BuzzHandler::OnActivation()
{
    return std::nullopt;
}
