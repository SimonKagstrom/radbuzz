#include "ble_server_host.hh"

std::unique_ptr<ListenerCookie>
BleServerHost::AttachConnectionListener(std::function<void(bool connected)> cb)
{
    m_connection_listener = cb;

    return std::make_unique<ListenerCookie>([this]() { m_connection_listener = [](auto) {}; });
}

void
BleServerHost::SetServiceUuid128(hal::Uuid128Span service_uuid)
{
}

void
BleServerHost::AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                           std::function<void(std::span<const uint8_t>)> data)
{
}

void
BleServerHost::AddNotifyGattCharacteristics(hal::Uuid128Span uuid)
{
}

bool
BleServerHost::Notify(hal::Uuid128Span uuid, std::span<const uint8_t> data)
{
    return true;
}

void
BleServerHost::Start()
{
    m_connection_listener(true);
}

void
BleServerHost::PollEvents()
{
}
