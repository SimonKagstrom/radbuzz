#pragma once

#include "hal/i_ble_server.hh"

class BleServerHost : public hal::IBleServer
{
public:
private:
    void SetServiceUuid128(std::span<const uint8_t, 16> service_uuid) final;

    void AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;

    void Start() final;
    void PollEvents() final;
};
