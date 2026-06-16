#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "os/memory.hh"

#include <etl/circular_buffer.h>
#include <mutex>
#include <utility>

class TripComputer : public os::BaseThread
{
public:
    struct TripLogEntry
    {
        Point position;
        milliseconds timestamp;
        int16_t power;
    };

    // 2MB of trip log entries
    static constexpr auto kNumberOfTripLogEntries = (2 * 1024 * 1024) / sizeof(TripLogEntry);
    using TripLog = etl::circular_buffer<TripLogEntry, kNumberOfTripLogEntries>;


    explicit TripComputer(ApplicationState& app_state);

    std::pair<std::unique_lock<etl::mutex>, TripLog&> GetLog();

private:
    std::optional<milliseconds> OnActivation() final;

    void UpdateSoc(uint16_t millivolts);

    ApplicationState& m_state;

    std::unique_ptr<ListenerCookie> m_state_listener;
    os::TimerHandle m_soc_timer;

    uint32_t m_current_distance {0};

    etl::circular_buffer<uint16_t, 10> m_millivolt_history;
    os::mem_unique_ptr<TripLog> m_trip_log;

    etl::mutex m_log_mutex;
};
