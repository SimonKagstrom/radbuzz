#pragma once

#include "listener_cookie.hh"
#include "uuid_helper.hh"

#include <cstdint>
#include <functional>
#include <memory>

namespace hal
{

class IBleServer
{
public:
    virtual ~IBleServer() = default;

    virtual std::unique_ptr<ListenerCookie>
    AttachConnectionListener(std::function<void(bool connected)> cb) = 0;

    virtual void SetServiceUuid128(Uuid128Span service_uuid) = 0;

    virtual void
    AddWriteGattCharacteristics(Uuid128Span uuid,
                                std::function<void(std::span<const uint8_t>)> data_cb) = 0;

    /** @brief Start the BLE server
     */
    virtual void Start() = 0;

    /**
     * Poll BLE events.
     * 
     * Should be called periodically
     */
    virtual void PollEvents() = 0;
};

} // namespace hal
