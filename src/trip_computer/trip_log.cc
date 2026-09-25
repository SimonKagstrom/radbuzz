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
TripLog<Entries>::StaleAndReplace(LogHandle current_entry_handle)
{
    auto handle = m_parent.AllocateLogEntry();
    if (!handle)
    {
        return std::nullopt;
    }

    auto& current_entry = m_parent.WritableEntry(current_entry_handle);

    m_parent.WritableEntry(*handle) = current_entry;

    Link(current_entry.predecessor, *handle);
    Link(*handle, current_entry.successor);


    current_entry.stale = true;

    return handle;
}

template <size_t Entries>
void
TripLog<Entries>::RecalculateNeighbor(LogHandle neighbor_handle)
{
    // The first entry is always kept (max area), so it never needs a new queue entry
    if (neighbor_handle == kInvalidLogHandle ||
        m_parent.Entry(neighbor_handle).predecessor == kInvalidLogHandle)
    {
        return;
    }

    // The pending entry is not in the queue yet, and gets its area calculated when pushed
    if (neighbor_handle == m_pending_log_entry->handle)
    {
        return;
    }

    auto replacement = StaleAndReplace(neighbor_handle);
    debug_assert(replacement);

    m_log_queue.push(
        {.triangle_area = TriangleArea(m_parent.Entry(*replacement)), .handle = *replacement});
}

template <size_t Entries>
void
TripLog<Entries>::RebuildQueue()
{
    // Drop everything, freeing the stale entries (live entries are still in the list)
    while (!m_log_queue.empty())
    {
        const auto handle = m_log_queue.top().handle;
        m_log_queue.pop();

        if (m_parent.Entry(handle).stale)
        {
            m_parent.FreeLogEntry(handle);
        }
    }

    // Everything before the pending entry belongs in the queue
    auto current = m_parent.Entry(m_pending_log_entry->handle).predecessor;
    while (current != kInvalidLogHandle)
    {
        const auto& entry = m_parent.Entry(current);
        m_log_queue.push({.triangle_area = TriangleArea(entry), .handle = current});
        current = entry.predecessor;
    }
}

template <size_t Entries>
void
TripLog<Entries>::Link(LogHandle predecessor, LogHandle successor)
{
    if (predecessor != kInvalidLogHandle)
    {
        m_parent.WritableEntry(predecessor).successor = successor;
        debug_assert(successor == kInvalidLogHandle || m_parent.Entry(successor).stale == false);
    }

    if (successor != kInvalidLogHandle)
    {
        m_parent.WritableEntry(successor).predecessor = predecessor;
        debug_assert(predecessor == kInvalidLogHandle || m_parent.Entry(predecessor).stale == false);
    }
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
                              .stale = false,
                              .power = power,
                              .predecessor = kInvalidLogHandle,
                              .successor = kInvalidLogHandle};

    if (m_pending_log_entry)
    {
        // Update the successor of the current pending entry
        Link(m_pending_log_entry->handle, *handle);

        if (m_entry_count >= Entries)
        {
            while (m_parent.Entry(m_log_queue.top().handle).stale)
            {
                m_parent.FreeLogEntry(m_log_queue.top().handle);
                m_log_queue.pop();

                debug_assert(!m_log_queue.empty() &&
                             "the log queue can't contain of only stale entries");
            }

            // A removal pushes at most two replacements, and the pending entry is pushed below
            if (m_log_queue.size() + 3 > m_log_queue.max_size())
            {
                RebuildQueue();
            }

            // Copy, since pushing to the queue changes what top() refers to
            const auto to_remove_handle = m_log_queue.top().handle;
            m_log_queue.pop();

            const auto& entry_to_remove = m_parent.Entry(to_remove_handle);
            debug_assert(entry_to_remove.predecessor != kInvalidLogHandle &&
                         "Can't remove the first entry");

            const auto predecessor_handle = entry_to_remove.predecessor;
            const auto successor_handle = entry_to_remove.successor;

            Link(predecessor_handle, successor_handle);
            m_parent.FreeLogEntry(to_remove_handle);

            RecalculateNeighbor(successor_handle);
            RecalculateNeighbor(predecessor_handle);
        }

        // Calculated after the removal, since that might have changed the predecessor
        m_pending_log_entry->triangle_area =
            TriangleArea(m_parent.Entry(m_pending_log_entry->handle));
        m_log_queue.push(*m_pending_log_entry);
        m_pending_log_entry = LogQueueEntry {0, *handle};
        m_entry_count = std::min(m_entry_count + 1u, static_cast<decltype(m_entry_count)>(Entries));
    }
    else
    {
        debug_assert(m_log_queue.empty());

        // This is the first entry, will be fixed up above
        m_pending_log_entry = LogQueueEntry {0, *handle};
        m_entry_count = 1;

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

    if (m_pending_log_entry)
    {
        m_parent.FreeLogEntry(m_pending_log_entry->handle);
    }

    m_log_queue = {};
    m_pending_log_entry.reset();
    m_entry_count = 0;
}


template class TripLog<TripComputer::kNumberOfExportLogEntries>;
template class TripLog<TripComputer::kNumberOfDisplayLogEntries>;

// Unit tests
template class TripLog<4>;
template class TripLog<6>;
//template class TripLog<8>;
