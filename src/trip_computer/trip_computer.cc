#include "trip_computer.hh"

#include <numeric>

namespace
{

constexpr auto kMillivoltSocTable = std::array {std::pair<uint16_t, uint8_t> {3270, 0},
                                                std::pair<uint16_t, uint8_t> {3390, 10},
                                                std::pair<uint16_t, uint8_t> {3510, 20},
                                                std::pair<uint16_t, uint8_t> {3610, 30},
                                                std::pair<uint16_t, uint8_t> {3680, 40},
                                                std::pair<uint16_t, uint8_t> {3740, 50},
                                                std::pair<uint16_t, uint8_t> {3820, 60},
                                                std::pair<uint16_t, uint8_t> {3930, 70},
                                                std::pair<uint16_t, uint8_t> {4050, 80},
                                                std::pair<uint16_t, uint8_t> {4130, 90},
                                                std::pair<uint16_t, uint8_t> {4200, 100}};

uint8_t
InterpolateSoc(uint16_t millivolts, uint8_t battery_series)
{
    millivolts /= battery_series;

    if (millivolts <= kMillivoltSocTable.front().first)
    {
        return kMillivoltSocTable.front().second;
    }
    if (millivolts >= kMillivoltSocTable.back().first)
    {
        return kMillivoltSocTable.back().second;
    }

    for (size_t i = 1; i < kMillivoltSocTable.size(); ++i)
    {
        const auto& [mv_low, soc_low] = kMillivoltSocTable[i - 1];
        const auto& [mv_high, soc_high] = kMillivoltSocTable[i];

        if (millivolts < mv_high)
        {
            float ratio = static_cast<float>(millivolts - mv_low) / (mv_high - mv_low);
            return static_cast<uint8_t>(soc_low + ratio * (soc_high - soc_low));
        }
    }

    // Should never reach here
    return 0;
}
} // namespace

TripComputer::TripComputer(ApplicationState& app_state)
    : m_state(app_state)
    , m_state_listener(
          m_state.AttachListener<AS::configuration, AS::pixel_position>(GetSemaphore()))
    , m_trip_log(os::AllocSlowMem<etl::circular_buffer<TripLogEntry, kNumberOfTripLogEntries>>())
{
    m_soc_timer = StartTimer(250ms, [this]() {
        auto mv = m_state.CheckoutReadonly().Get<AS::battery_millivolts>();
        if (mv != 0)
        {
            UpdateSoc(mv);
        }
        return 250ms;
    });
}

void
TripComputer::UpdateSoc(uint16_t millivolts)
{
    auto battery_cell_series =
        m_state.CheckoutReadonly().Get<AS::configuration>()->battery_cell_series;
    if (battery_cell_series == 0)
    {
        // Invalid / incomplete configuration
        return;
    }

    if (m_millivolt_history.empty())
    {
        m_state.CheckoutReadWrite().Set<AS::battery_soc>(
            InterpolateSoc(millivolts, battery_cell_series));
    }

    auto ro = m_state.CheckoutReadonly();

    auto distance_now = ro.Get<AS::distance_traveled>();
    auto qw =
        m_state
            .CheckoutQueuedWriter<AS::is_moving, AS::battery_soc, AS::battery_milliamphours_left>();

    qw.Set<AS::is_moving>(distance_now - m_current_distance > 10);
    m_current_distance = distance_now;


    if (ro.Get<AS::current_power_w>() > 300)
    {
        // Limit SOC updates on high power
        return;
    }

    m_millivolt_history.push(millivolts);
    if (m_millivolt_history.full())
    {
        auto soc = InterpolateSoc(
            std::accumulate(m_millivolt_history.begin(), m_millivolt_history.end(), 0u) /
                m_millivolt_history.size(),
            battery_cell_series);

        qw.Set<AS::battery_soc>(soc);
        // Convert Ah to mAh and apply SOC
        qw.Set<AS::battery_milliamphours_left>(ro.Get<AS::configuration>()->battery_amp_hours *
                                               1000 * soc / 100);

        // Discard the oldest
        m_millivolt_history.pop();
    }
}

std::pair<std::unique_lock<etl::mutex>, TripComputer::TripLog&>
TripComputer::GetLog()
{
    auto lock = std::unique_lock(m_log_mutex);
    return {std::move(lock), *m_trip_log};
}

std::optional<milliseconds>
TripComputer::OnActivation()
{
    auto ro = m_state.CheckoutReadonly();

    if (!ro.Get<AS::gps_position_valid>())
    {
        return std::nullopt;
    }

    auto lock = std::lock_guard(m_log_mutex);
    auto position = *ro.Get<AS::pixel_position>();
    auto now = os::GetTimeStamp();
    if (m_trip_log->empty())
    {
        m_trip_log->push(TripLogEntry {position, now, ro.Get<AS::current_power_w>()});
    }
    else
    {
        const auto& last = m_trip_log->back();
        auto max_diff = std::max(std::abs(position.x - last.position.x),
                                 std::abs(position.y - last.position.y));

        if (max_diff > 500)
        {
            // GPS jump
            m_trip_log->clear();
            m_trip_log->push(TripLogEntry {position, now, ro.Get<AS::current_power_w>()});
        }
        else if (max_diff > 5)
        {
            m_trip_log->push(TripLogEntry {position, now, ro.Get<AS::current_power_w>()});
        }
    }

    return std::nullopt;
}
