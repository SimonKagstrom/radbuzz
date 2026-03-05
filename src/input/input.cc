#include "input.hh"

Input::Input(hal::IGpio& button, RotaryEncoder& rotary_encoder)
{
    m_button_listener_cookie = button.AttachIrqListener(
        [this](bool state) { m_on_event(state ? EventType::kButtonDown : EventType::kButtonUp); });

    m_rotary_listener_cookie =
        rotary_encoder.AttachIrqListener([this](RotaryEncoder::Direction direction) {
            m_on_event(direction == RotaryEncoder::Direction::kLeft ? EventType::kLeft
                                                                    : EventType::kRight);
        });
}

std::unique_ptr<ListenerCookie>
Input::AttachListener(std::function<void(EventType)> on_event)
{
    m_on_event = std::move(on_event);

    return std::make_unique<ListenerCookie>([this]() { m_on_event = [](auto) { /* nop */ }; });
}
