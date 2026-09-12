#pragma once

#include "application_state.hh"

#include <etl/circular_buffer.h>
#include <etl/priority_queue.h>
#include <etl/vector.h>

using LogHandle = uint16_t;
using TriangleAreaType = uint16_t;
constexpr LogHandle kInvalidLogHandle = std::numeric_limits<LogHandle>::max();

struct TripLogEntry
{
    milliseconds timestamp;
    Point position;
    bool stale;
    decltype(AS::current_power_w::current_power_w) power;
    LogHandle predecessor;
    LogHandle successor;
};

class IEntryAllocator
{
public:
    virtual ~IEntryAllocator() = default;

    virtual std::optional<LogHandle> AllocateLogEntry() = 0;
    virtual void FreeLogEntry(LogHandle handle) = 0;

    virtual TripLogEntry& WritableEntry(LogHandle handle) = 0;
    virtual const TripLogEntry& Entry(LogHandle handle) const = 0;
};

template <size_t Entries>
class TripLog
{
public:
    TripLog(IEntryAllocator& parent)
        : m_parent(parent)
    {
    }

    struct LogQueueEntry
    {
        TriangleAreaType triangle_area;
        LogHandle handle;

        int operator<(const LogQueueEntry& other) const
        {
            if (triangle_area == other.triangle_area)
            {
                return rand() % 2; // Randomize order of entries with the same area to avoid bias
            }
            // We want the entry with the smallest triangle area to be popped first, so we invert the comparison here
            return triangle_area > other.triangle_area;
        }
    };

    std::optional<LogHandle> AddEntry(const Point& position, milliseconds timestamp, int16_t power);

    std::optional<LogHandle> GetLastHandle() const
    {
        if (m_pending_log_entry)
        {
            return m_pending_log_entry->handle;
        }

        return std::nullopt;
    }

    void Reset();

private:
    TriangleAreaType TriangleArea(const TripLogEntry& entry) const;

    IEntryAllocator& m_parent;

    etl::priority_queue<LogQueueEntry, Entries> m_log_queue;
    std::optional<LogQueueEntry> m_pending_log_entry;
};
