#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "os/memory.hh"

#include <etl/circular_buffer.h>
#include <etl/priority_queue.h>
#include <mutex>
#include <optional>
#include <utility>

class TripComputer : public os::BaseThread
{
public:
    using LogHandle = uint16_t;
    struct TripLogEntry
    {
        Point position;
        milliseconds timestamp;
        int16_t power;
        LogHandle predecessor;
        LogHandle successor;
    };


    // x MB of trip log entries
    static constexpr auto kNumberOfTripLogEntries = (1 * 1024 * 1024) / sizeof(TripLogEntry);
    using TripLog = etl::circular_buffer<TripLogEntry, kNumberOfTripLogEntries>;

    explicit TripComputer(ApplicationState& app_state);

    std::pair<std::unique_lock<etl::mutex>, TripLog&> GetLog();

    const TripLogEntry& Entry(LogHandle handle) const
    {
        return (*m_trip_log_storage)[handle];
    }

private:
    struct LogQueueEntry
    {
        uint32_t triangle_area;
        LogHandle handle;

        auto operator<(const LogQueueEntry& other) const
        {
            // We want the entry with the smallest triangle area to be popped first, so we invert the comparison here
            return triangle_area > other.triangle_area;
        }
    };

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void UpdateSoc(uint16_t millivolts);

    std::optional<LogHandle> AllocateLogEntry();
    void FreeLogEntry(LogHandle handle);
    uint32_t TriangleArea(const Point& a, const Point& b, const Point& c) const;


    TripLogEntry& WritableEntry(LogHandle handle)
    {
        return (*m_trip_log_storage)[handle];
    }


    ApplicationState& m_state;

    std::unique_ptr<ListenerCookie> m_state_listener;
    os::TimerHandle m_soc_timer;

    uint32_t m_current_distance {0};

    etl::circular_buffer<uint16_t, 10> m_millivolt_history;
    os::mem_unique_ptr<TripLog> m_trip_log;

    os::mem_unique_ptr<std::array<TripLogEntry, kNumberOfTripLogEntries>> m_trip_log_storage;
    std::vector<LogHandle> m_free_log_entries;
    etl::priority_queue<LogQueueEntry, 64> m_log_queue;

    std::optional<LogQueueEntry> m_pending_log_entry;

    etl::mutex m_log_mutex;
};
