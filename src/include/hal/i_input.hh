#pragma once

#include "listener_cookie.hh"

#include <cstdint>
#include <functional>

namespace hal
{

class IInput
{
public:
    enum class EventType : uint8_t
    {
        kNoEvent = 0,

        kButtonDown,
        kButtonUp,
        kLeft,
        kRight,

        kTouchDown,
        kTouchUp,
        kTouchMove,

        kValueCount,
    };

    struct Event
    {
        EventType type;
        uint16_t x;
        uint16_t y;
    };

    virtual ~IInput() = default;

    virtual std::unique_ptr<ListenerCookie> AttachListener(std::function<void(Event)> on_event) = 0;
};

} // namespace hal
