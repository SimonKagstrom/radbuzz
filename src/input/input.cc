#include "input.hh"

Input::Input(hal::IGpio& button, RotaryEncoder& rotary_encoder, hal::ITouch& touch)
    : m_touch(touch)
{
    m_button_listener_cookie = button.AttachIrqListener([this](bool state) {
        m_button_queue.push(state);
        Awake();
    });

    m_rotary_listener_cookie =
        rotary_encoder.AttachIrqListener([this](RotaryEncoder::Direction direction) {
            m_encoder_queue.push(direction);
            Awake();
        });

    m_touch_listener_cookie = touch.AttachIrqListener([this]() { Awake(); });
    if (!m_touch_listener_cookie)
    {
        // If the touch doesn't support IRQ listeners, we'll just poll it in the main loop
        m_poll_interval = 50ms;
    }
}

std::unique_ptr<ListenerCookie>
Input::AttachListener(std::function<void(Event)> on_event)
{
    m_on_event = std::move(on_event);

    return std::make_unique<ListenerCookie>([this]() { m_on_event = [](auto) { /* nop */ }; });
}

std::optional<milliseconds>
Input::OnActivation()
{
    bool button_state;
    RotaryEncoder::Direction direction;

    while (m_button_queue.pop(button_state))
    {
        m_on_event({button_state ? EventType::kButtonDown : EventType::kButtonUp, 0, 0});
    }
    while (m_encoder_queue.pop(direction))
    {
        m_on_event(
            {direction == RotaryEncoder::Direction::kLeft ? EventType::kLeft : EventType::kRight,
             0,
             0});
    }

    auto touch_data = m_touch.GetActiveTouchData();
    for (const auto& data : touch_data)
    {
        EventType touch_event_type;
        if (!data.pressed)
        {
            touch_event_type = EventType::kTouchUp;
        }
        else if (data.was_pressed)
        {
            touch_event_type = EventType::kTouchMove;
        }
        else
        {
            touch_event_type = EventType::kTouchDown;
        }
        m_on_event({touch_event_type, data.x, data.y});
    }

    return m_poll_interval;
}