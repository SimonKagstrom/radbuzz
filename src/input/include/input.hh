#pragma once

#include "base_thread.hh"
#include "hal/i_gpio.hh"
#include "hal/i_input.hh"
#include "hal/i_touch.hh"
#include "rotary_encoder.hh"

class Input : public os::BaseThread, public hal::IInput
{
public:
    Input(hal::IGpio& button, RotaryEncoder& rotary_encoder, hal::ITouch& touch);

    std::unique_ptr<ListenerCookie>
    AttachListener(std::function<void(EventType, uint16_t, uint16_t)> on_event) final;

private:
    std::optional<milliseconds> OnActivation() final;

    hal::ITouch& m_touch;

    std::function<void(EventType, uint16_t, uint16_t)> m_on_event {
        [](auto, auto, auto) { /* NOP */ }};
    std::unique_ptr<ListenerCookie> m_button_listener_cookie;
    std::unique_ptr<ListenerCookie> m_rotary_listener_cookie;
    std::unique_ptr<ListenerCookie> m_touch_listener_cookie;
};
