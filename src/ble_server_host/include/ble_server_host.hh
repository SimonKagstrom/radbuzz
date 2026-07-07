#pragma once

#include "ble_injector.hh"
#include "hal/i_ble_server.hh"

#include <cassert>
#include <etl/queue_spsc_atomic.h>
#include <unordered_map>

class BleServerHost : public hal::IBleServer, public BleInjector
{
private:
    std::unique_ptr<ListenerCookie>
    AttachConnectionListener(std::function<void(bool connected)> cb) final;

    void SetServiceUuid128(hal::Uuid128Span service_uuid) final;

    void AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;

    void Start() final;
    void PollEvents() final;

    void OnInjection(hal::Uuid128Span uuid, std::span<const uint8_t> data) final;

    std::unordered_map<uint8_t, std::function<void(std::span<const uint8_t>)>> m_uuid_cb;

    std::function<void(bool)> m_connection_listener {[](auto) {}};
};
