#pragma once

#include "hal/i_ble_server.hh"

#include <cassert>
#include <etl/queue_spsc_atomic.h>
#include <unordered_map>

class BleServerHost : public hal::IBleServer
{
public:
    void Inject(auto uuid, const std::string& data)
    {
        Inject(uuid, {reinterpret_cast<const uint8_t*>(data.data()), data.size()});
    }

    void Inject(auto uuid, std::span<const uint8_t> data)
    {
        auto uuid128 = hal::detail::StringToUuid128(uuid);

        assert(m_uuid_cb.find(uuid128[0]) != m_uuid_cb.end());

        m_event_queue.push({uuid128[0], std::vector<uint8_t>(data.begin(), data.end())});
    }

private:
    struct Event
    {
        uint8_t uuid;
        std::vector<uint8_t> data;
    };

    void SetServiceUuid128(std::span<const uint8_t, 16> service_uuid) final;

    void AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;

    void Start() final;
    void PollEvents() final;

    std::unordered_map<uint8_t, std::function<void(std::span<const uint8_t>)>> m_uuid_cb;
    etl::queue_spsc_atomic<Event, 16> m_event_queue;
};
