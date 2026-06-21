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

std::pair<std::unique_lock<etl::mutex>, std::span<const TripComputer::DisplayTripLogEntry>>
TripComputer::GetLog()
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

uint32_t
TripComputer::TriangleArea(const Point& a, const Point& b, const Point& c) const
{
    debug_assert(a.zoom == kDefaultZoom && b.zoom == kDefaultZoom && c.zoom == kDefaultZoom);

    // Only the relation is important, not the absolute area
    const auto doubled_area = (a.x) * (b.y - c.y) + (b.x) * (c.y - a.y) + (c.x) * (a.y - b.y);
    return std::abs(doubled_area);
}

uint32_t
TripComputer::TriangleArea(const TripLogEntry& entry) const
{
    if (entry.predecessor == kInvalidLogHandle)
    {
        return std::numeric_limits<uint32_t>::max();
    }
    debug_assert(entry.successor != kInvalidLogHandle);

    return TriangleArea(
        Entry(entry.predecessor).position, entry.position, Entry(entry.successor).position);
}

std::optional<milliseconds>
TripComputer::OnActivation()
{
    auto ro = m_state.CheckoutReadonly();

    if (!ro.Get<AS::gps_position_valid>())
    {
        return std::nullopt;
    }

    auto position = *ro.Get<AS::pixel_position>();

    if (m_pending_log_entry &&
        std::abs(position.x - Entry(m_pending_log_entry->handle).position.x) < 5 &&
        std::abs(position.y - Entry(m_pending_log_entry->handle).position.y) < 5)
    {
        // Wait for a position further away
        return std::nullopt;
    }

    auto now = os::GetTimeStamp();

    auto handle = AllocateLogEntry();

    // We should have enough entries, so for now just assert
    debug_assert(handle);

    auto& new_entry = WritableEntry(*handle);

    new_entry = TripLogEntry {
        position, now, ro.Get<AS::current_power_w>(), kInvalidLogHandle, kInvalidLogHandle};

    if (m_pending_log_entry)
    {
        // Update the successor of the current pending entry
        auto& last_entry = WritableEntry(m_pending_log_entry->handle);
        last_entry.successor = *handle;
        new_entry.predecessor = m_pending_log_entry->handle;
        m_pending_log_entry->triangle_area = TriangleArea(last_entry);

        if (m_log_queue.full())
        {
            const auto& to_remove = m_log_queue.top();

            auto& entry_to_remove = Entry(to_remove.handle);
            debug_assert(entry_to_remove.predecessor != kInvalidLogHandle &&
                         "Can't remove the first entry");
            WritableEntry(entry_to_remove.predecessor).successor = entry_to_remove.successor;
            if (entry_to_remove.successor != kInvalidLogHandle)
            {
                WritableEntry(entry_to_remove.successor).predecessor = entry_to_remove.predecessor;
            }

            m_free_log_entries.push_back(to_remove.handle);
            m_log_queue.pop();
        }

        m_log_queue.push(*m_pending_log_entry);
        m_pending_log_entry = LogQueueEntry {0, *handle};


        auto lock = std::lock_guard(m_log_mutex);
        // Swap first, so that this is fresh when the lock is released
        m_current_display_log = !m_current_display_log;
        auto& display_log = m_display_logs[m_current_display_log];

        display_log.clear();
        display_log.push_back(DisplayTripLogEntry {position, ro.Get<AS::current_power_w>()});
        auto pred_handle = new_entry.predecessor;
        while (pred_handle != kInvalidLogHandle)
        {
            const auto& pred_entry = Entry(pred_handle);
            display_log.push_back(DisplayTripLogEntry {pred_entry.position, pred_entry.power});
            pred_handle = pred_entry.predecessor;
        }
    }
    else
    {
        debug_assert(m_log_queue.empty());

        // This is the first entry, will be fixed up above
        m_pending_log_entry = LogQueueEntry {0, *handle};
    }

    return std::nullopt;
}
