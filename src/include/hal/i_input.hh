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


    virtual ~IInput() = default;

    virtual std::unique_ptr<ListenerCookie>
    AttachListener(std::function<void(EventType, uint16_t, uint16_t)> on_event) = 0;
};

} // namespace hal
