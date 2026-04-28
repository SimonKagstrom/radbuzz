#pragma once

#include "base_thread.hh"
#include "hal/i_gpio.hh"
#include "hal/i_input.hh"
#include "hal/i_touch.hh"
#include "rotary_encoder.hh"

#include <etl/queue_spsc_atomic.h>

class Input : public os::BaseThread, public hal::IInput
{
public:
    Input(hal::IGpio& button, RotaryEncoder& rotary_encoder, hal::ITouch& touch);

    std::unique_ptr<ListenerCookie> AttachListener(std::function<void(Event)> on_event) final;

private:
    std::optional<milliseconds> OnActivation() final;

    hal::ITouch& m_touch;

    std::function<void(Event)> m_on_event {[](auto) { /* NOP */ }};
    std::unique_ptr<ListenerCookie> m_button_listener_cookie;
    std::unique_ptr<ListenerCookie> m_rotary_listener_cookie;
    std::unique_ptr<ListenerCookie> m_touch_listener_cookie;

    std::optional<milliseconds> m_poll_interval {std::nullopt};

    etl::queue_spsc_atomic<bool, 4> m_button_queue;
    etl::queue_spsc_atomic<RotaryEncoder::Direction, 4> m_encoder_queue;
};
