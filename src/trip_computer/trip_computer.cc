#include "trip_computer.hh"

#include "battery_utils.hh"
#include "debug_assert.hh"

#include <numeric>

namespace
{

// Limit in watts below which the battery sensing is done
constexpr auto kBatterySenseLimit = 100;

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

TripComputer::TripComputer(ApplicationState& app_state, PostOffice<MSG::AllMessages>& post_office)
    : m_state(app_state)
    , m_state_listener(m_state.AttachListener<AS::configuration,
                                              AS::can_bus_active,
                                              AS::odometer,
                                              AS::pixel_position>(*this))
    , m_trip_log_storage(std::make_unique<std::array<TripLogEntry, kNumberOfTripLogEntries>>())
{
    m_mailbox = post_office.Subscribe<MSG::reset_trip>(*this);

    // Fill with zeroes to start from the rightmost point
    for (auto i = 0; i < kNumberOfRecentEntries; ++i)
    {
        m_recent_entries.push(RecentEntry {0});
    }
}

void
TripComputer::OnStartup()
{
    m_free_log_entries.reserve(kNumberOfTripLogEntries);
    for (auto i = 0; i < kNumberOfTripLogEntries; i++)
    {
        m_free_log_entries.push_back(i);
    }

    m_soc_timer = StartTimer(100ms, [this]() {
        std::optional<milliseconds> out = 100ms;
        if (m_state.Get<AS::can_bus_active>())
        {
            StartMonitoring();

            out = std::nullopt;
        }

        return out;
    });
}

void
TripComputer::StartMonitoring()
{
    ResetTrip();

    // The first histogram entry is relative to where we start, not from zero
    m_current_histogram_entry.start_consumption =
        m_state.Get<AS::wh_consumed>() - m_state.Get<AS::wh_regenerated>();
    m_current_histogram_entry.start_distance = m_state.Get<AS::odometer>();

    m_soc_timer = StartTimer(250ms, [this]() {
        if (!m_state.Get<AS::bms_data>()->valid)
        {
            auto mv = m_state.Get<AS::battery_millivolts>();

            if (mv != 0)
            {
                UpdateSoc(mv);
            }
        }

        auto odometer = m_state.Get<AS::odometer>();

        UpdateSpeedAndTime(odometer);
        UpdateRange();
        UpdateRecentEntries(odometer);
        m_current_distance = odometer;

        return 250ms;
    });
}


TripComputer::DistanceType
TripComputer::RecentDistance(TripComputer::DistanceType distance) const
{
    auto resolution = m_state.Get<AS::configuration>()->recent_power_distance;

    // Round to the nearest X meters
    return (distance / resolution) * resolution;
}

void
TripComputer::UpdateRecentEntries(uint32_t odometer)
{
    auto current_distance = RecentDistance(m_current_distance);
    auto distance_now = RecentDistance(odometer);
    auto power = m_state.Get<AS::current_power_w>();
    auto consumed = m_state.Get<AS::wh_consumed>() - m_state.Get<AS::wh_regenerated>();

    if (distance_now != current_distance)
    {
        if (m_recent_entries.full())
        {
            m_recent_entries.pop();
        }

        // Start a new entry with the current power. There's no distance to measure the
        // consumption over yet, so it starts out empty
        m_recent_entries.push({std::max(power, static_cast<PowerType>(0)), 0.0f});

        m_current_distance = distance_now;
        m_current_histogram_entry = {};

        // Store the current consumption and distance
        m_current_histogram_entry.start_consumption = consumed;
        m_current_histogram_entry.start_distance = odometer;
    }
    else
    {
        m_current_histogram_entry.accumulated_power += power;
        m_current_histogram_entry.samples++;

        PowerType average_power = std::max(m_current_histogram_entry.accumulated_power /
                                               m_current_histogram_entry.samples,
                                           static_cast<int32_t>(0));

        float average_consumption = 0.0f;

        if (odometer - m_current_histogram_entry.start_distance != 0)
        {
            average_consumption = (consumed - m_current_histogram_entry.start_consumption) *
                                  (1000.0f / (odometer - m_current_histogram_entry.start_distance));
        }

        average_consumption = std::min(average_consumption, 100.0f);

        // Live update of the current entry
        m_recent_entries.back().power = average_power;
        m_recent_entries.back().average_consumption = average_consumption;
    }
}


void
TripComputer::UpdateRange()
{
    auto soc = m_state.Get<AS::battery_soc>();
    if (m_last_soc == soc)
    {
        // No need to update the range
        return;
    }

    m_last_soc = soc;

    const auto conf = m_state.Get<AS::configuration>();
    const uint8_t wh_per_km = std::max<uint8_t>(1, conf->wh_per_km_for_range_estimation);

    auto full_pack_wh = battery::FullPackWh(conf);
    const float wh_left = full_pack_wh * (static_cast<float>(soc) / 100.0f);
    uint32_t range = std::max(1.0f, wh_left / static_cast<float>(wh_per_km));

    m_state.Set<AS::estimated_range_km>(range);
}

void
TripComputer::ResetTrip()
{
    m_trip_start_distance = m_state.Get<AS::odometer>();

    auto qw = m_state.CheckoutQueuedWriter<AS::trip_distance,
                                           AS::trip_duration,
                                           AS::trip_average_speed,
                                           AS::is_moving,
                                           AS::trip_max_speed>();
    qw.Set<AS::trip_distance>(0);
    qw.Set<AS::trip_duration>(0s);
    qw.Set<AS::trip_average_speed>(0);
    qw.Set<AS::is_moving>(false);
    // Set by the can bus handler
    qw.Set<AS::trip_max_speed>(0);

    m_display_logs[0].clear();
    m_display_logs[1].clear();
    m_current_display_log = 0;
    //m_export_log.Reset();
    m_display_log.Reset();

    m_current_distance = m_trip_start_distance;
    m_current_trip_movement_second = std::chrono::duration_cast<seconds>(os::GetTimeStamp());
}

void
TripComputer::UpdateSoc(uint16_t millivolts)
{
    auto battery_cell_series = m_state.Get<AS::configuration>()->battery_cell_series;

    if (battery_cell_series == 0)
    {
        // Invalid / incomplete configuration
        return;
    }

    if (m_millivolt_history.empty() && m_state.Get<AS::demo_mode>() == false)
    {
        m_state.Set<AS::battery_soc>(InterpolateSoc(millivolts, battery_cell_series));
    }

    if (m_state.Get<AS::current_power_w>() > kBatterySenseLimit)
    {
        // Limit SOC updates on high power
        return;
    }

    m_millivolt_history.push(millivolts);
    if (m_millivolt_history.full())
    {
        auto qw = m_state.CheckoutQueuedWriter<AS::battery_soc>();

        auto soc = InterpolateSoc(
            std::accumulate(m_millivolt_history.begin(), m_millivolt_history.end(), 0u) /
                m_millivolt_history.size(),
            battery_cell_series);

        qw.Set<AS::battery_soc>(soc);

        // Discard the oldest
        m_millivolt_history.pop();
    }
}

std::pair<std::unique_lock<etl::mutex>, std::span<const TripComputer::DisplayTripLogEntry>>
TripComputer::GetDisplayLog()
{
    auto lock = std::unique_lock(m_log_mutex);
    return {std::move(lock), m_display_logs[m_current_display_log]};
}

std::span<const TripComputer::RecentEntry>
TripComputer::GetRecentEntries()
{
    m_display_recent_entries = {};
    m_display_recent_entries.clear();

    std::lock_guard<etl::mutex> lock(m_log_mutex);
    for (const auto& entry : m_recent_entries)
    {
        m_display_recent_entries.push_back(entry);
    }

    return m_display_recent_entries;
}


std::optional<LogHandle>
TripComputer::AllocateLogEntry()
{
    if (m_free_log_entries.empty())
    {
        return std::nullopt;
    }

    auto handle = m_free_log_entries.back();
    m_free_log_entries.pop_back();

    return handle;
}

void
TripComputer::FreeLogEntry(LogHandle handle)
{
    debug_assert(handle != kInvalidLogHandle);

    m_free_log_entries.push_back(handle);
}

std::optional<milliseconds>
TripComputer::OnActivation()
{
    m_mailbox->Collect().On<MSG::reset_trip>([this]() { ResetTrip(); });

    UpdateTripLog();

    return std::nullopt;
}

void
TripComputer::UpdateSpeedAndTime(uint32_t distance_now)
{
    auto qw = m_state.CheckoutQueuedWriter<AS::is_moving,
                                           AS::trip_duration,
                                           AS::trip_distance,
                                           AS::trip_average_speed>();

    auto is_moving_now = distance_now != m_current_distance;
    auto is_moving = m_state.Get<AS::is_moving>();
    auto now = std::chrono::duration_cast<seconds>(os::GetTimeStamp());

    if (is_moving_now)
    {
        if (!is_moving)
        {
            // Take the current value into account
            m_current_trip_movement_second = now - m_state.Get<AS::trip_duration>();
        }

        is_moving = true;
        qw.Set<AS::is_moving>(true);

        // Restart the cancel timer
        m_moving_timer = StartTimer(5s, [this]() {
            m_state.Set<AS::is_moving>(false);
            return std::nullopt;
        });
    }

    if (is_moving)
    {
        auto trip_duration = now - m_current_trip_movement_second;
        auto trip_distance = distance_now - m_trip_start_distance;

        qw.Set<AS::trip_duration>(trip_duration);
        qw.Set<AS::trip_distance>(trip_distance);

        if (trip_duration.count() != 0)
        {
            qw.Set<AS::trip_average_speed>(
                static_cast<uint8_t>(trip_distance * 3.6f / trip_duration.count()));
        }
        else
        {
            // Shouldn't happen, but why take a chance?
            qw.Set<AS::trip_average_speed>(0);
        }
    }
}

void
TripComputer::UpdateTripLog()
{
    if (m_state.Get<AS::gps_status>() != GpsStatus::kPositionValid)
    {
        return;
    }

    auto position = *m_state.Get<AS::pixel_position>();
    auto now = os::GetTimeStamp();
    auto power = m_state.Get<AS::current_power_w>();

    //    m_export_log.AddEntry(position, now, power);
    auto new_entry_handle = m_display_log.AddEntry(position, now, power);

    if (new_entry_handle.has_value())
    {
        const auto& new_entry = Entry(*new_entry_handle);

        auto update_log = !m_current_display_log;
        auto& display_log = m_display_logs[update_log];

        display_log.clear();
        display_log.push_back(DisplayTripLogEntry {position, m_state.Get<AS::current_power_w>()});
        auto pred_handle = new_entry.predecessor;
        while (pred_handle != kInvalidLogHandle)
        {
            const auto& pred_entry = Entry(pred_handle);
            display_log.push_back(DisplayTripLogEntry {pred_entry.position, pred_entry.power});
            pred_handle = pred_entry.predecessor;
        }

        auto lock = std::lock_guard(m_log_mutex);
        m_current_display_log = update_log;
    }
}
