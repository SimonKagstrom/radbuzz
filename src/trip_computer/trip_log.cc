#include "trip_log.hh"

#include "trip_computer.hh"

#include <numeric>

static_assert(TripComputer::kNumberOfTripLogEntries <= std::numeric_limits<LogHandle>::max(),
              "Handle type too small for number of entries");

namespace
{

TriangleAreaType
TriangleArea(const Point& a, const Point& b, const Point& c)
{
    debug_assert(a.zoom == kDefaultZoom && b.zoom == kDefaultZoom && c.zoom == kDefaultZoom);

    // Only the relation is important, not the absolute area
    const auto doubled_area = (a.x) * (b.y - c.y) + (b.x) * (c.y - a.y) + (c.x) * (a.y - b.y);
    static_assert(sizeof(doubled_area) > sizeof(TriangleAreaType));

    const auto abs_doubled_area = std::abs(doubled_area);

    // Saturate the area for large values (triangles should normally be small)
    if (abs_doubled_area > std::numeric_limits<TriangleAreaType>::max())
    {
        return std::numeric_limits<TriangleAreaType>::max();
    }

    return abs_doubled_area;
}

} // namespace

template <size_t Entries>
TriangleAreaType
TripLog<Entries>::TriangleArea(const TripLogEntry& entry) const
{
    if (entry.predecessor == kInvalidLogHandle)
    {
        return std::numeric_limits<TriangleAreaType>::max();
    }
    debug_assert(entry.successor != kInvalidLogHandle);

    return ::TriangleArea(m_parent.Entry(entry.predecessor).position,
                          entry.position,
                          m_parent.Entry(entry.successor).position);
}

template <size_t Entries>
std::optional<LogHandle>
TripLog<Entries>::AddEntry(const Point& position, milliseconds timestamp, int16_t power)
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

    new_entry = TripLogEntry {.timestamp = timestamp,
                              .position = position,
                              .power = power,
                              .predecessor = kInvalidLogHandle,
                              .successor = kInvalidLogHandle};

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

        static_assert(sizeof(LogQueueEntry) == 4);
    }

    return *handle;
}


template <size_t Entries>
void
TripLog<Entries>::Reset()
{
    while (!m_log_queue.empty())
    {
        auto& entry = m_log_queue.top();
        m_parent.FreeLogEntry(entry.handle);
        m_log_queue.pop();
    }

    m_log_queue = {};
    m_pending_log_entry.reset();
}


template class TripLog<TripComputer::kNumberOfExportLogEntries>;
template class TripLog<TripComputer::kNumberOfDisplayLogEntries>;
