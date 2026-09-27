#pragma once

#include "hal/i_ble_server.hh"

/// A BLE server which does nothing, for use with the demo mode
class BleServerHost : public hal::IBleServer
{
private:
    std::unique_ptr<ListenerCookie>
    AttachConnectionListener(std::function<void(bool connected)> cb) final;

    void SetServiceUuid128(hal::Uuid128Span service_uuid) final;

    void AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;

    void AddNotifyGattCharacteristics(hal::Uuid128Span uuid) final;

    bool Notify(hal::Uuid128Span uuid, std::span<const uint8_t> data) final;

    void Start() final;
    void PollEvents() final;

    std::function<void(bool)> m_connection_listener {[](auto) {}};
};
