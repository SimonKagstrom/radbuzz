#include "input.hh"

Input::Input(hal::IGpio& button, RotaryEncoder& rotary_encoder, hal::ITouch& touch)
    : m_touch(touch)
{
    m_button_listener_cookie = button.AttachIrqListener([this](bool state) {
        m_on_event(state ? EventType::kButtonDown : EventType::kButtonUp, 0, 0);
    });

    m_rotary_listener_cookie =
        rotary_encoder.AttachIrqListener([this](RotaryEncoder::Direction direction) {
            m_on_event(direction == RotaryEncoder::Direction::kLeft ? EventType::kLeft
                                                                    : EventType::kRight,
                       0,
                       0);
        });

    m_touch_listener_cookie = touch.AttachIrqListener([this]() { Awake(); });
}

std::unique_ptr<ListenerCookie>
Input::AttachListener(std::function<void(EventType, uint16_t, uint16_t)> on_event)
{
    m_on_event = std::move(on_event);

    return std::make_unique<ListenerCookie>(
        [this]() { m_on_event = [](auto, auto, auto) { /* nop */ }; });
}

std::optional<milliseconds>
Input::OnActivation()
{
    auto touch_data = m_touch.GetActiveTouchData();
    for (const auto& data : touch_data)
    {
        m_on_event(data.pressed ? EventType::kTouchDown
                                : (data.was_pressed ? EventType::kTouchUp : EventType::kTouchMove),
                   data.x,
                   data.y);
    }

    return 50ms;
}