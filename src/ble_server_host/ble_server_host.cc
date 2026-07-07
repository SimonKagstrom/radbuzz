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
    // For now assume they are unique
    assert(m_uuid_cb.find(uuid[0]) == m_uuid_cb.end());

    m_uuid_cb[uuid[0]] = data;
}

void
BleServerHost::Start()
{
    m_connection_listener(true);
}

void
BleServerHost::PollEvents()
{
    PollInjections();
}

void
BleServerHost::OnInjection(hal::Uuid128Span uuid, std::span<const uint8_t> data)
{
    if (m_uuid_cb.find(uuid[0]) == m_uuid_cb.end())
    {
        return;
    }

    m_uuid_cb[uuid[0]]({data.data(), data.size()});
}
