#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "os/memory.hh"
#include "trip_computer.hh"
#include "trip_log.hh"
#include "wgs84_to_osm_point.hh"

#include <etl/circular_buffer.h>
#include <etl/priority_queue.h>
#include <etl/vector.h>
#include <mutex>
#include <optional>
#include <utility>

class TripComputer : public os::BaseThread, public IEntryAllocator
{
public:
    using PowerType = decltype(AS::current_power_w::current_power_w);
    using DistanceType = decltype(AS::odometer::odometer);

    struct DisplayTripLogEntry
    {
        Point position;
        PowerType power;
    };


    struct RecentEntry
    {
        PowerType power;
        float average_consumption;
    };


    // x MB of trip log entries
    static constexpr auto kNumberOfTripLogEntries = (128 * 1024) / sizeof(TripLogEntry);
    static constexpr auto kNumberOfDisplayLogEntries = 128;
    static constexpr auto kNumberOfExportLogEntries =
        kNumberOfTripLogEntries - kNumberOfDisplayLogEntries - 2;

    static constexpr auto kNumberOfRecentEntries = 10;

    explicit TripComputer(ApplicationState& app_state);

    std::pair<std::unique_lock<etl::mutex>, std::span<const DisplayTripLogEntry>> GetDisplayLog();
    std::span<const RecentEntry> GetRecentEntries();

    const TripLogEntry& Entry(LogHandle handle) const final
    {
        return (*m_trip_log_storage)[handle];
    }

private:
    struct RecentHistogramEntry
    {
        int32_t accumulated_power;
        float start_consumption;
        float start_distance;
        int32_t samples;
    };


    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void StartMonitoring();
    void UpdateSoc(uint16_t millivolts);
    void UpdateTripLog();
    void UpdateSpeedAndTime(uint32_t odometer);
    void UpdateRange();
    void UpdateRecentEntries(uint32_t odometer);
    void ResetTrip();

    DistanceType RecentDistance(DistanceType distance) const;

    std::optional<LogHandle> AllocateLogEntry() final;
    void FreeLogEntry(LogHandle handle) final;


    TripLogEntry& WritableEntry(LogHandle handle) final
    {
        return (*m_trip_log_storage)[handle];
    }


    ApplicationState& m_state;

    std::unique_ptr<ListenerCookie> m_state_listener;
    ApplicationState::PartialReadOnlyCache<AS::reset_trip> m_state_cache;
    os::TimerHandle m_soc_timer;
    uint8_t m_last_soc {0};

    os::TimerHandle m_moving_timer;
    uint32_t m_trip_start_distance {0};
    uint32_t m_current_distance {0};
    seconds m_current_trip_movement_second;

    etl::circular_buffer<uint16_t, 10> m_millivolt_history;

    std::unique_ptr<std::array<TripLogEntry, kNumberOfTripLogEntries>> m_trip_log_storage;
    std::vector<LogHandle> m_free_log_entries;

    TripLog<kNumberOfDisplayLogEntries> m_display_log {*this};
    TripLog<kNumberOfExportLogEntries> m_export_log {*this};

    std::array<std::vector<DisplayTripLogEntry>, 2> m_display_logs;
    std::atomic<uint8_t> m_current_display_log {0};


    etl::circular_buffer<RecentEntry, kNumberOfRecentEntries> m_recent_entries {};
    etl::vector<RecentEntry, kNumberOfRecentEntries> m_display_recent_entries;

    RecentHistogramEntry m_current_histogram_entry {};

    etl::mutex m_log_mutex;
};
