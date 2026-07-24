#include "trip_computer.hh"

#include "debug_assert.hh"

#include <numeric>

static_assert(TripComputer::kNumberOfTripLogEntries <=
                  std::numeric_limits<TripComputer::LogHandle>::max(),
              "Handle type too small for number of entries");
static constexpr TripComputer::LogHandle kInvalidLogHandle =
    std::numeric_limits<TripComputer::LogHandle>::max();

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


uint32_t
TriangleArea(const Point& a, const Point& b, const Point& c)
{
    debug_assert(a.zoom == kDefaultZoom && b.zoom == kDefaultZoom && c.zoom == kDefaultZoom);

    // Only the relation is important, not the absolute area
    const auto doubled_area = (a.x) * (b.y - c.y) + (b.x) * (c.y - a.y) + (c.x) * (a.y - b.y);
    return std::abs(doubled_area);
}


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
    , m_state_listener(m_state.AttachListener<AS::configuration,
                                              AS::can_bus_active,
                                              AS::odometer,
                                              AS::pixel_position>(GetSemaphore()))
    , m_trip_log_storage(std::make_unique<std::array<TripLogEntry, kNumberOfTripLogEntries>>())
{
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
        if (m_state.CheckoutReadonly().Get<AS::can_bus_active>())
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

    m_soc_timer = StartTimer(250ms, [this]() {
        auto ro = m_state.CheckoutReadonly();
        auto mv = ro.Get<AS::battery_millivolts>();

        if (mv != 0)
        {
            UpdateSoc(mv);
        }

        UpdateSpeedAndTime();

        return 250ms;
    });
}

void
TripComputer::ResetTrip()
{
    auto rw = m_state.CheckoutReadWrite();
    m_trip_start_distance = rw.Get<AS::odometer>();

    rw.Set<AS::trip_distance>(0);
    rw.Set<AS::trip_duration>(0s);

    m_current_distance = m_trip_start_distance;
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
    auto qw = m_state.CheckoutQueuedWriter<AS::battery_soc, AS::battery_milliamphours_left>();


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

std::pair<std::unique_lock<etl::mutex>, std::span<const TripComputer::DisplayTripLogEntry>>
TripComputer::GetDisplayLog()
{
    auto lock = std::unique_lock(m_log_mutex);
    return {std::move(lock), m_display_logs[m_current_display_log]};
}

std::optional<TripComputer::LogHandle>
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
    UpdateTripLog();

    return std::nullopt;
}

void
TripComputer::UpdateSpeedAndTime()
{
    auto rw = m_state.CheckoutReadWrite();
    auto distance_now = rw.Get<AS::odometer>();

    auto is_moving_now = distance_now != m_current_distance;
    auto now = std::chrono::duration_cast<seconds>(os::GetTimeStamp());

    if (is_moving_now)
    {
        if (!rw.Get<AS::is_moving>())
        {
            // Take the current value into account
            m_current_trip_movement_second = now - rw.Get<AS::trip_duration>();
        }

        rw.Set<AS::is_moving>(true);

        // Restart the cancel timer
        m_moving_timer = StartTimer(5s, [this]() {
            auto rw = m_state.CheckoutReadWrite();
            rw.Set<AS::is_moving>(false);
            return std::nullopt;
        });
    }

    m_current_distance = distance_now;
    if (rw.Get<AS::is_moving>())
    {
        auto trip_duration = now - m_current_trip_movement_second;
        auto trip_distance = m_current_distance - m_trip_start_distance;

        rw.Set<AS::trip_duration>(trip_duration);
        rw.Set<AS::trip_distance>(trip_distance);

        if (trip_duration.count() != 0)
        {
            rw.Set<AS::trip_average_speed>(
                static_cast<uint8_t>(trip_distance * 3.6f / trip_duration.count()));
        }
        else
        {
            // Shouldn't happen, but why take a chance?
            rw.Set<AS::trip_average_speed>(0);
        }
    }
}

void
TripComputer::UpdateTripLog()
{
    auto ro = m_state.CheckoutReadonly();
    if (!ro.Get<AS::gps_position_valid>())
    {
        return;
    }

    auto position = *ro.Get<AS::pixel_position>();
    auto now = os::GetTimeStamp();
    auto power = ro.Get<AS::current_power_w>();

    m_export_log.AddEntry(position, now, power);
    auto new_entry_handle = m_display_log.AddEntry(position, now, power);

    if (new_entry_handle.has_value())
    {
        const auto& new_entry = Entry(*new_entry_handle);

        auto update_log = !m_current_display_log;
        auto& display_log = m_display_logs[update_log];

        display_log.clear();
        display_log.push_back(DisplayTripLogEntry {position, ro.Get<AS::current_power_w>()});
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

template <size_t Entries>
uint32_t
TripComputer::Log<Entries>::TriangleArea(const TripLogEntry& entry) const
{
    if (entry.predecessor == kInvalidLogHandle)
    {
        return std::numeric_limits<uint32_t>::max();
    }
    debug_assert(entry.successor != kInvalidLogHandle);

    return ::TriangleArea(m_parent.Entry(entry.predecessor).position,
                          entry.position,
                          m_parent.Entry(entry.successor).position);
}

template <size_t Entries>
std::optional<TripComputer::LogHandle>
TripComputer::Log<Entries>::AddEntry(const Point& position, milliseconds timestamp, int16_t power)
{
    if (m_pending_log_entry &&
        std::abs(position.x - m_parent.Entry(m_pending_log_entry->handle).position.x) < 5 &&
        std::abs(position.y - m_parent.Entry(m_pending_log_entry->handle).position.y) < 5)
    {
        // Wait for a position further away
        return std::nullopt;
    }
    auto handle = m_parent.AllocateLogEntry();

    // We should have enough entries, so for now just assert
    debug_assert(handle);

    auto& new_entry = m_parent.WritableEntry(*handle);

    new_entry = TripComputer::TripLogEntry {
        position, timestamp, power, kInvalidLogHandle, kInvalidLogHandle};

    if (m_pending_log_entry)
    {
        // Update the successor of the current pending entry
        auto& last_entry = m_parent.WritableEntry(m_pending_log_entry->handle);
        last_entry.successor = *handle;
        new_entry.predecessor = m_pending_log_entry->handle;
        m_pending_log_entry->triangle_area = TriangleArea(last_entry);

        if (m_log_queue.full())
        {
            const auto& to_remove = m_log_queue.top();

            auto& entry_to_remove = m_parent.Entry(to_remove.handle);
            debug_assert(entry_to_remove.predecessor != kInvalidLogHandle &&
                         "Can't remove the first entry");
            m_parent.WritableEntry(entry_to_remove.predecessor).successor =
                entry_to_remove.successor;
            if (entry_to_remove.successor != kInvalidLogHandle)
            {
                m_parent.WritableEntry(entry_to_remove.successor).predecessor =
                    entry_to_remove.predecessor;
            }

            m_parent.FreeLogEntry(to_remove.handle);
            m_log_queue.pop();
        }

        m_log_queue.push(*m_pending_log_entry);
        m_pending_log_entry = LogQueueEntry {0, *handle};
    }
    else
    {
        debug_assert(m_log_queue.empty());

        // This is the first entry, will be fixed up above
        m_pending_log_entry = LogQueueEntry {0, *handle};
    }

    return *handle;
}
