#include "ble_server_host.hh"

void
BleServerHost::SetServiceUuid128(std::span<const uint8_t, 16> service_uuid)
{
}

void
BleServerHost::AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                           std::function<void(std::span<const uint8_t>)> data)
{
}

void
BleServerHost::Start()
{
}

void
BleServerHost::PollEvents()
{
}
