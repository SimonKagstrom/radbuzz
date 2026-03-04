#pragma once

#include "hal/i_gpio.hh"
#include "hal/i_input.hh"
#include "rotary_encoder.hh"

class InputEsp32 : public hal::IInput
{
public:
    InputEsp32(hal::IGpio& button, RotaryEncoder& rotary_encoder);

    std::unique_ptr<ListenerCookie> AttachListener(std::function<void(EventType)> on_event) final;

private:
    std::function<void(EventType)> m_on_event {[](auto) { /* NOP */ }};
    std::unique_ptr<ListenerCookie> m_button_listener_cookie;
    std::unique_ptr<ListenerCookie> m_rotary_listener_cookie;
};
