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

    if (current_entry.predecessor != kInvalidLogHandle)
    {
        printf("Staling entry %u(%u, %u) -> %u(%u, %u)",
               current_entry.predecessor,
               m_parent.Entry(current_entry.predecessor).position.x,
               m_parent.Entry(current_entry.predecessor).position.y,
               current_entry_handle,
               current_entry.position.x,
               current_entry.position.y);

        if (current_entry.successor != kInvalidLogHandle)
        {
            printf(" -> %u(%u, %u)",
                   current_entry.successor,
                   m_parent.Entry(current_entry.successor).position.x,
                   m_parent.Entry(current_entry.successor).position.y);
        }

        printf("\n");
    }
    else if (current_entry.successor != kInvalidLogHandle)
    {
        printf("staling successor only -> (%u, %u) -> (%u, %u)\n",
               current_entry.position.x,
               current_entry.position.y,
               m_parent.Entry(current_entry.successor).position.x,
               m_parent.Entry(current_entry.successor).position.y);
    }
    Link(current_entry.predecessor, *handle);
    Link(*handle, current_entry.successor);


    current_entry.stale = true;

    if (current_entry_handle == m_pending_log_entry->handle)
    {
        m_pending_log_entry->handle = *handle;
    }

    return handle;
}

template <size_t Entries>
void
TripLog<Entries>::Link(LogHandle predecessor, LogHandle successor)
{
    if (predecessor != kInvalidLogHandle)
    {
        m_parent.WritableEntry(predecessor).successor = successor;
        if (successor != kInvalidLogHandle)
            debug_assert(m_parent.Entry(successor).stale == false);
    }

    if (successor != kInvalidLogHandle)
    {
        m_parent.WritableEntry(successor).predecessor = predecessor;
        if (predecessor != kInvalidLogHandle)
        {
            if (m_parent.Entry(predecessor).stale)
                printf("Entry at (%u, %u) is stale\n",
                       m_parent.Entry(predecessor).position.x,
                       m_parent.Entry(predecessor).position.y);
            debug_assert(m_parent.Entry(predecessor).stale == false);
        }
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

    printf("Adding entry at position %u(%u, %u) with predecessor %u\n", *handle,position.x, position.y, m_pending_log_entry ? m_pending_log_entry->handle : kInvalidLogHandle);

    if (m_pending_log_entry)
    {
        // Update the successor of the current pending entry
        Link(m_pending_log_entry->handle, *handle);
        m_pending_log_entry->triangle_area =
            TriangleArea(m_parent.Entry(m_pending_log_entry->handle));

        if (m_entry_count >= Entries)
        {
            while (m_parent.Entry(m_log_queue.top().handle).stale)
            {
                m_parent.FreeLogEntry(m_log_queue.top().handle);
                auto& e = m_parent.Entry(m_log_queue.top().handle);
                printf("Pruning entry at (%d,%d)!\n", e.position.x, e.position.y);
                m_log_queue.pop();

                debug_assert(!m_log_queue.empty() &&
                             "the log queue can't contain of only stale entries");
            }

            const auto& to_remove = m_log_queue.top();

            auto& entry_to_remove = m_parent.Entry(to_remove.handle);
            debug_assert(entry_to_remove.predecessor != kInvalidLogHandle &&
                         "Can't remove the first entry");
            printf("Removing entry at %u(%d,%d)\n",
                   to_remove.handle,
                   entry_to_remove.position.x,
                   entry_to_remove.position.y);

            Link(entry_to_remove.predecessor, entry_to_remove.successor);

            auto new_predecessor = StaleAndReplace(entry_to_remove.predecessor);
            debug_assert(new_predecessor);

            if (entry_to_remove.successor != kInvalidLogHandle)
            {
                auto new_successor = StaleAndReplace(entry_to_remove.successor);
                debug_assert(new_successor);

                Link(*new_predecessor, *new_successor);

                printf(" Calculating successor (%d,%d)\n",
                       m_parent.Entry(*new_successor).position.x,
                       m_parent.Entry(*new_successor).position.y);
                m_log_queue.push({.triangle_area = TriangleArea(m_parent.Entry(*new_successor)),
                                  .handle = *new_successor});
            }

            m_log_queue.push({.triangle_area = TriangleArea(m_parent.Entry(*new_predecessor)),
                              .handle = *new_predecessor});
            printf(" Calculatating predecessor (%d,%d)\n",
                   m_parent.Entry(*new_predecessor).position.x,
                   m_parent.Entry(*new_predecessor).position.y);

            m_parent.FreeLogEntry(to_remove.handle);
            m_log_queue.pop();
        }

        m_log_queue.push(*m_pending_log_entry);
        m_pending_log_entry = LogQueueEntry {0, *handle};
        m_entry_count = std::min(m_entry_count + 1u, static_cast<decltype(m_entry_count)>(Entries));
    }
    else
    {
        debug_assert(m_log_queue.empty());

        // This is the first entry, will be fixed up above
        m_pending_log_entry = LogQueueEntry {0, *handle};
        printf("Initial add att %d,%d\n",
               m_parent.Entry(*handle).position.x,
               m_parent.Entry(*handle).position.y);
        m_entry_count = 1;

        static_assert(sizeof(LogQueueEntry) == 4);
    }

    printf("DONE. List now\n");
    auto pred_handle = *handle;
    while (pred_handle != kInvalidLogHandle)
    {
        const auto& pred_entry = m_parent.Entry(pred_handle);
        printf(" %u(%u, %u) ->", pred_handle, pred_entry.position.x, pred_entry.position.y);
        pred_handle = pred_entry.predecessor;
    }
    printf("\n");
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

// Unit tests
template class TripLog<4>;
template class TripLog<6>;
//template class TripLog<8>;
