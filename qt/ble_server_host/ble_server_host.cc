#include "ble_server_host.hh"

void
BleServerHost::SetServiceUuid128(std::span<const uint8_t, 16> service_uuid)
{
}

void
BleServerHost::AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                           std::function<void(std::span<const uint8_t>)> data)
{
    // For now assume they are unique
    assert(m_uuid_cb.find(uuid[0]) == m_uuid_cb.end());

    m_uuid_cb[uuid[0]] = data;
}

void
BleServerHost::Start()
{
}

void
BleServerHost::PollEvents()
{
    Event ev;

    while (m_event_queue.pop(ev))
    {
        // Presense verified in the Inject method
        m_uuid_cb[ev.uuid]({ev.data.data(), ev.data.size()});
    }
}
