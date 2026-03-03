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

        kValueCount,
    };


    virtual ~IInput() = default;

    virtual std::unique_ptr<ListenerCookie>
    AttachListener(std::function<void(EventType)> on_event) = 0;
};

} // namespace hal
