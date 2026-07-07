#pragma once

#include "hal/uuid_helper.hh"

#include <cassert>
#include <etl/queue_spsc_atomic.h>
#include <string>
#include <unordered_map>

class BleInjector
{
public:
    void Inject(auto uuid, const std::string& data)
    {
        Inject(uuid, {reinterpret_cast<const uint8_t*>(data.data()), data.size()});
    }

    void Inject(auto uuid, std::span<const uint8_t> data)
    {
        auto uuid128 = hal::detail::StringToUuid128(uuid);

        m_event_queue.push({uuid128, std::vector<uint8_t>(data.begin(), data.end())});
    }

protected:
    void PollInjections();

    virtual void OnInjection(hal::Uuid128Span uuid, std::span<const uint8_t> data) = 0;

private:
    struct Event
    {
        hal::Uuid128 uuid;
        std::vector<uint8_t> data;
    };

    etl::queue_spsc_atomic<Event, 16> m_event_queue;
};