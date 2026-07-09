#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "os/memory.hh"
#include "wgs84_to_osm_point.hh"

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

    struct DisplayTripLogEntry
    {
        Point position;
        int16_t power;
    };


    // x MB of trip log entries
    static constexpr auto kNumberOfTripLogEntries = (128 * 1024) / sizeof(TripLogEntry);
    static constexpr auto kNumberOfDisplayLogEntries = 64;
    static constexpr auto kNumberOfExportLogEntries =
        kNumberOfTripLogEntries - kNumberOfDisplayLogEntries - 2;

    explicit TripComputer(ApplicationState& app_state);

    std::pair<std::unique_lock<etl::mutex>, std::span<const DisplayTripLogEntry>> GetDisplayLog();

    const TripLogEntry& Entry(LogHandle handle) const
    {
        return (*m_trip_log_storage)[handle];
    }

private:
    template <size_t Entries>
    class Log
    {
    public:
        Log(TripComputer& parent)
            : m_parent(parent)
        {
        }

        struct LogQueueEntry
        {
            uint32_t triangle_area;
            LogHandle handle;

            int operator<(const LogQueueEntry& other) const
            {
                if (triangle_area == other.triangle_area)
                {
                    return rand() %
                           2; // Randomize order of entries with the same area to avoid bias
                }
                // We want the entry with the smallest triangle area to be popped first, so we invert the comparison here
                return triangle_area > other.triangle_area;
            }
        };

        std::optional<LogHandle>
        AddEntry(const Point& position, milliseconds timestamp, int16_t power);

        std::optional<LogHandle> GetLastHandle() const
        {
            if (m_pending_log_entry)
            {
                return m_pending_log_entry->handle;
            }

            return std::nullopt;
        }

    private:
        uint32_t TriangleArea(const TripLogEntry& entry) const;

        TripComputer& m_parent;

        etl::priority_queue<LogQueueEntry, Entries> m_log_queue;
        std::optional<LogQueueEntry> m_pending_log_entry;
    };

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void UpdateSoc(uint16_t millivolts);

    std::optional<LogHandle> AllocateLogEntry();
    void FreeLogEntry(LogHandle handle);


    TripLogEntry& WritableEntry(LogHandle handle)
    {
        return (*m_trip_log_storage)[handle];
    }


    ApplicationState& m_state;

    std::unique_ptr<ListenerCookie> m_state_listener;
    os::TimerHandle m_soc_timer;

    uint32_t m_current_distance {0};

    etl::circular_buffer<uint16_t, 10> m_millivolt_history;

    std::unique_ptr<std::array<TripLogEntry, kNumberOfTripLogEntries>> m_trip_log_storage;
    std::vector<LogHandle> m_free_log_entries;

    Log<kNumberOfDisplayLogEntries> m_display_log {*this};
    Log<kNumberOfExportLogEntries> m_export_log {*this};

    std::array<std::vector<DisplayTripLogEntry>, 2> m_display_logs;
    std::atomic<uint8_t> m_current_display_log {0};

    etl::mutex m_log_mutex;
};
