#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "os/memory.hh"

#include <etl/circular_buffer.h>

class TripComputer : public os::BaseThread
{
public:
    explicit TripComputer(ApplicationState& app_state);

private:
    struct TripLogEntry
    {
        Point position;
        milliseconds timestamp;
        int16_t power;
    };

    // 2MB of trip log entries
    static constexpr auto kNumberOfTripLogEntries = (2 * 1024 * 1024) /  sizeof(TripLogEntry);

    std::optional<milliseconds> OnActivation() final;

    void UpdateSoc(uint16_t millivolts);

    ApplicationState& m_state;

    std::unique_ptr<ListenerCookie> m_state_listener;
    os::TimerHandle m_soc_timer;

    uint32_t m_current_distance {0};

    etl::circular_buffer<uint16_t, 10> m_millivolt_history;
    os::mem_unique_ptr<etl::circular_buffer<TripLogEntry, kNumberOfTripLogEntries>> m_trip_log;
};
