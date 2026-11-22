#include "trip_compactor.hh"

#include "trip_log.hh"
#include "trip_recorder.hh"
#include "wgs84_to_osm_point.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <limits>
#include <nlohmann/json.hpp>
#include <vector>

namespace
{

// Bytes read from the raw trip at a time
constexpr auto kReadSize = 1024uz;

// The Web Mercator limits
constexpr auto kMaxLatitude = 85.0511f;
constexpr auto kMaxLongitude = 180.0f;

// Live entries, plus stale replacements still in the TripLog queue, plus some margin
constexpr auto kAllocatorEntries = TripCompactor::kMaxEntries * 2 + 4;

struct RawPoint
{
    uint32_t utc;
    GpsPosition position;
    int16_t power;
};

class EntryAllocator : public IEntryAllocator
{
public:
    EntryAllocator()
        : m_entries(kAllocatorEntries)
    {
        m_free_entries.reserve(kAllocatorEntries);
        for (auto i = 0uz; i < kAllocatorEntries; i++)
        {
            m_free_entries.push_back(i);
        }
    }

    std::optional<LogHandle> AllocateLogEntry() final
    {
        if (m_free_entries.empty())
        {
            return std::nullopt;
        }

        auto handle = m_free_entries.back();
        m_free_entries.pop_back();

        return handle;
    }

    void FreeLogEntry(LogHandle handle) final
    {
        m_free_entries.push_back(handle);
    }

    TripLogEntry& WritableEntry(LogHandle handle) final
    {
        return m_entries[handle];
    }

    const TripLogEntry& Entry(LogHandle handle) const final
    {
        return m_entries[handle];
    }

private:
    std::vector<TripLogEntry> m_entries;
    std::vector<LogHandle> m_free_entries;
};

std::string
RawPath(std::string_view root, std::string_view name)
{
    return std::format("{}/{}", TripRecorder::Directory(root, TripRecorder::kRawDirectory), name);
}

std::string
LogPath(std::string_view root, std::string_view name)
{
    return std::format("{}/{}", TripRecorder::Directory(root, TripCompactor::kLogDirectory), name);
}

// The values used for compaction, or nothing if the line is broken
std::optional<RawPoint>
Parse(std::string_view line)
{
    // No exceptions on target, so check every type before getting a value
    auto json = nlohmann::json::parse(line, nullptr, false);
    if (!json.is_object())
    {
        return std::nullopt;
    }

    auto utc = json.find("utc");
    auto latitude = json.find("lat");
    auto longitude = json.find("lon");
    auto power = json.find("power");

    if (utc == json.end() || !utc->is_number_unsigned() ||
        utc->get<uint64_t>() > std::numeric_limits<uint32_t>::max() || latitude == json.end() ||
        !latitude->is_number() || longitude == json.end() || !longitude->is_number())
    {
        return std::nullopt;
    }

    auto out = RawPoint {.utc = utc->get<uint32_t>(),
                         .position = {latitude->get<float>(), longitude->get<float>()},
                         .power = 0};

    // Outside of what the map can show
    if (std::abs(out.position.latitude) > kMaxLatitude ||
        std::abs(out.position.longitude) > kMaxLongitude)
    {
        return std::nullopt;
    }

    if (power != json.end() && power->is_number_integer())
    {
        out.power = static_cast<int16_t>(std::clamp<int64_t>(power->get<int64_t>(),
                                                             std::numeric_limits<int16_t>::min(),
                                                             std::numeric_limits<int16_t>::max()));
    }

    return out;
}

// Call on_line for each line of a file, without the newline. False if it can't be opened
template <typename OnLine>
bool
ForEachLine(const Filesystem& filesystem, std::string_view path, OnLine on_line)
{
    auto file = filesystem.OpenReadOnly(path);
    if (!file)
    {
        return false;
    }

    std::string pending;
    while (true)
    {
        auto size = pending.size();
        pending.resize(size + kReadSize);

        auto read = file->Read(std::as_writable_bytes(std::span(pending).subspan(size)));
        pending.resize(size + read);
        if (read == 0)
        {
            break;
        }

        auto start = 0uz;
        for (auto end = pending.find('\n'); end != std::string::npos;
             end = pending.find('\n', start))
        {
            on_line(std::string_view(pending).substr(start, end - start));
            start = end + 1;
        }
        pending.erase(0, start);
    }

    // An unterminated last line, e.g., from a power cut while writing
    if (!pending.empty())
    {
        on_line(std::string_view(pending));
    }

    return true;
}

} // namespace

TripCompactor::TripCompactor(PostOffice<MSG::AllMessages>& post_office, Filesystem& filesystem)
    : m_post_office(post_office)
    , m_filesystem(filesystem)
    , m_mailbox(m_post_office.Subscribe<MSG::new_recorded_trip>(*this))
{
}

void
TripCompactor::OnStartup()
{
    WalkRawDirectory();
}

std::optional<milliseconds>
TripCompactor::OnActivation()
{
    m_mailbox->Collect().On<MSG::new_recorded_trip>([this]() { WalkRawDirectory(); });

    return std::nullopt;
}

void
TripCompactor::WalkRawDirectory()
{
    for (auto root : TripRecorder::kRoots)
    {
        for (const auto& entry :
             m_filesystem.ListDirectory(TripRecorder::Directory(root, TripRecorder::kRawDirectory)))
        {
            if (entry.is_directory)
            {
                continue;
            }

            auto result = Compact(root, entry.name);
            if (result == CompactResult::kFailed)
            {
                // Left in RAW, and retried on the next new trip
                printf("TripCompactor: can't compact %s/%s\n", root, entry.name.c_str());
                continue;
            }

            if (result == CompactResult::kWritten)
            {
                m_post_office.Send<MSG::new_compacted_trip>();
            }

            if (!m_filesystem.Remove(RawPath(root, entry.name)))
            {
                printf("TripCompactor: can't remove %s/%s\n", root, entry.name.c_str());
            }
        }
    }
}

TripCompactor::CompactResult
TripCompactor::Compact(std::string_view root, std::string_view name)
{
    const auto raw_path = RawPath(root, name);

    // On the heap, since the TripLog queue alone is a few KB
    auto allocator = std::make_unique<EntryAllocator>();
    auto log = std::make_unique<TripLog<kMaxEntries>>(*allocator);

    auto opened = ForEachLine(m_filesystem, raw_path, [&](auto line) {
        auto point = Parse(line);
        if (!point)
        {
            return;
        }

        log->AddEntry(
            *Wgs84ToOsmPoint(point->position, kDefaultZoom), seconds(point->utc), point->power);
    });

    if (!opened)
    {
        return CompactResult::kFailed;
    }

    // The kept points, from the last one and backwards
    std::vector<uint32_t> kept_utcs;
    for (auto handle = log->GetLastHandle().value_or(kInvalidLogHandle);
         handle != kInvalidLogHandle;
         handle = allocator->Entry(handle).predecessor)
    {
        kept_utcs.push_back(allocator->Entry(handle).timestamp.count());
    }
    std::ranges::reverse(kept_utcs);

    if (kept_utcs.empty())
    {
        // Nothing to keep, so done
        printf("TripCompactor: no points in %s\n", raw_path.c_str());
        return CompactResult::kNoPoints;
    }

    auto file = m_filesystem.OpenWritable(LogPath(root, name), Filesystem::WriteMode::kTruncate);
    if (!file)
    {
        return CompactResult::kFailed;
    }

    // Copy the kept lines unchanged, so the compacted trip has the same information
    auto next = kept_utcs.begin();
    auto written = true;
    ForEachLine(m_filesystem, raw_path, [&](auto line) {
        if (next == kept_utcs.end())
        {
            return;
        }

        auto point = Parse(line);
        if (point && point->utc == *next)
        {
            written = written && file->Write(std::as_bytes(std::span(line))) &&
                      file->Write(std::as_bytes(std::span("\n", 1)));
            ++next;
        }
    });

    if (!written || next != kept_utcs.end() || !file->Sync())
    {
        return CompactResult::kFailed;
    }

    return CompactResult::kWritten;
}
