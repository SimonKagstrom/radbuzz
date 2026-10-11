#include "tile_regions.hh"

#include <algorithm>
#include <cstring>

namespace
{

// Bump when the format changes
constexpr uint32_t kMagic = 0x52474e31; // RGN1

int32_t
TilesAtZoom(uint8_t zoom)
{
    return 1 << zoom;
}

uint32_t
TilesPerRegion(int32_t region_size)
{
    return static_cast<uint32_t>(region_size * region_size);
}

size_t
BitmapBytes(int32_t region_size)
{
    return (TilesPerRegion(region_size) + 7) / 8;
}

bool
IsSet(const std::vector<uint8_t>& bitmap, uint32_t index)
{
    return bitmap[index / 8] & (1u << (index % 8));
}

template <typename T>
void
Append(std::vector<std::byte>& out, T value)
{
    auto offset = out.size();

    out.resize(offset + sizeof(T));
    memcpy(out.data() + offset, &value, sizeof(T));
}

class Reader
{
public:
    explicit Reader(std::span<const std::byte> data)
        : m_data(data)
    {
    }

    template <typename T>
    std::optional<T> Read()
    {
        if (Remaining() < sizeof(T))
        {
            return std::nullopt;
        }

        T value;
        memcpy(&value, m_data.data() + m_offset, sizeof(T));
        m_offset += sizeof(T);

        return value;
    }

    std::optional<std::span<const std::byte>> ReadBytes(size_t count)
    {
        if (Remaining() < count)
        {
            return std::nullopt;
        }

        auto out = m_data.subspan(m_offset, count);
        m_offset += count;

        return out;
    }

    size_t Remaining() const
    {
        return m_data.size() - m_offset;
    }

private:
    std::span<const std::byte> m_data;
    size_t m_offset {0};
};

} // namespace


TileRegions::TileRegions(std::span<const ZoomConfiguration> configurations)
    : m_configurations(configurations.begin(), configurations.end())
    , m_last_visited(m_configurations.size())
{
}

const TileRegions::ZoomConfiguration*
TileRegions::Configuration(uint8_t zoom) const
{
    auto it = std::ranges::find(m_configurations, zoom, &ZoomConfiguration::zoom);

    return it == m_configurations.end() ? nullptr : &*it;
}

TileRegions::PendingRegion
TileRegions::CreateRegion(const RegionId& id, const ZoomConfiguration& configuration) const
{
    const auto size = configuration.region_size;
    const auto world_size = TilesAtZoom(id.zoom);

    PendingRegion region {id, std::vector<uint8_t>(BitmapBytes(size), 0)};

    // Regions at the edge of the world can be partly outside. Nothing to download there
    for (auto i = 0u; i < TilesPerRegion(size); i++)
    {
        auto x = id.x * size + static_cast<int32_t>(i) % size;
        auto y = id.y * size + static_cast<int32_t>(i) / size;

        if (x >= world_size || y >= world_size)
        {
            region.on_disk[i / 8] |= (1u << (i % 8));
            region.tiles_on_disk++;
        }
    }

    return region;
}

void
TileRegions::Visit(const Tile& tile)
{
    auto configuration = Configuration(tile.zoom);
    const auto world_size = TilesAtZoom(tile.zoom);

    if (!configuration || tile.x < 0 || tile.y < 0 || tile.x >= world_size ||
        tile.y >= world_size)
    {
        return;
    }

    auto& last_visited = m_last_visited[configuration - m_configurations.data()];
    if (last_visited == tile)
    {
        // Still on the same tile, so the regions are the same as last time
        return;
    }
    last_visited = tile;

    const auto size = configuration->region_size;
    const auto margin = configuration->margin;

    auto first_x = std::max<int32_t>(0, tile.x - margin) / size;
    auto last_x = std::min<int32_t>(world_size - 1, tile.x + margin) / size;
    auto first_y = std::max<int32_t>(0, tile.y - margin) / size;
    auto last_y = std::min<int32_t>(world_size - 1, tile.y + margin) / size;

    for (auto y = first_y; y <= last_y; y++)
    {
        for (auto x = first_x; x <= last_x; x++)
        {
            auto id = RegionId {tile.zoom, x, y};

            if (m_completed.contains(id))
            {
                continue;
            }

            auto it = std::ranges::find(m_pending, id, &PendingRegion::id);
            if (it != m_pending.end())
            {
                // Move to the front (most recently visited)
                std::rotate(m_pending.begin(), it, it + 1);
            }
            else
            {
                m_pending.insert(m_pending.begin(), CreateRegion(id, *configuration));
                m_dirty = true;
            }
        }
    }
}

std::optional<Tile>
TileRegions::NextMissingTile()
{
    for (auto& region : m_pending)
    {
        const auto size = Configuration(region.id.zoom)->region_size;

        while (region.cursor < TilesPerRegion(size))
        {
            auto index = region.cursor++;

            if (!IsSet(region.on_disk, index))
            {
                return Tile {region.id.x * size + static_cast<int32_t>(index) % size,
                             region.id.y * size + static_cast<int32_t>(index) / size,
                             region.id.zoom};
            }
        }
    }

    return std::nullopt;
}

void
TileRegions::TileOnDisk(const Tile& tile)
{
    auto configuration = Configuration(tile.zoom);
    if (!configuration || tile.x < 0 || tile.y < 0)
    {
        return;
    }

    const auto size = configuration->region_size;
    auto id = RegionId {tile.zoom, tile.x / size, tile.y / size};

    auto it = std::ranges::find(m_pending, id, &PendingRegion::id);
    if (it == m_pending.end())
    {
        // Not pending (e.g., a tile shown in the UI elsewhere), or already complete
        return;
    }

    auto index = (tile.y - id.y * size) * size + (tile.x - id.x * size);
    SetOnDisk(it - m_pending.begin(), static_cast<uint32_t>(index));
}

bool
TileRegions::SetOnDisk(size_t region_index, uint32_t tile_index)
{
    auto& region = m_pending[region_index];

    if (IsSet(region.on_disk, tile_index))
    {
        return false;
    }

    region.on_disk[tile_index / 8] |= (1u << (tile_index % 8));
    region.tiles_on_disk++;
    m_dirty = true;

    if (region.tiles_on_disk == TilesPerRegion(Configuration(region.id.zoom)->region_size))
    {
        m_completed.insert(region.id);
        m_pending.erase(m_pending.begin() + region_index);

        return true;
    }

    return false;
}

void
TileRegions::RestartRound()
{
    for (auto& region : m_pending)
    {
        region.cursor = 0;
    }
}

size_t
TileRegions::PendingRegionCount() const
{
    return m_pending.size();
}

size_t
TileRegions::CompletedRegionCount() const
{
    return m_completed.size();
}

bool
TileRegions::IsDirty() const
{
    return m_dirty;
}

void
TileRegions::ClearDirty()
{
    m_dirty = false;
}

std::vector<std::byte>
TileRegions::Serialize() const
{
    std::vector<std::byte> out;

    Append<uint32_t>(out, kMagic);
    Append<uint32_t>(out, static_cast<uint32_t>(m_pending.size()));
    Append<uint32_t>(out, static_cast<uint32_t>(m_completed.size()));

    for (const auto& region : m_pending)
    {
        Append<uint32_t>(out, region.id.zoom);
        Append<int32_t>(out, region.id.x);
        Append<int32_t>(out, region.id.y);
        Append<uint32_t>(out, static_cast<uint32_t>(region.on_disk.size()));

        auto bytes = std::as_bytes(std::span(region.on_disk));
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    for (const auto& id : m_completed)
    {
        Append<uint32_t>(out, id.zoom);
        Append<int32_t>(out, id.x);
        Append<int32_t>(out, id.y);
    }

    return out;
}

bool
TileRegions::Deserialize(std::span<const std::byte> data)
{
    Reader reader(data);

    auto magic = reader.Read<uint32_t>();
    auto pending_count = reader.Read<uint32_t>();
    auto completed_count = reader.Read<uint32_t>();

    if (!magic || *magic != kMagic || !pending_count || !completed_count)
    {
        return false;
    }

    std::vector<PendingRegion> pending;
    std::unordered_set<RegionId, RegionIdHash> completed;

    // Return the region id if valid, or nullopt if not configured (e.g., the configuration has
    // changed since it was written). False on corrupt data
    auto read_id = [this, &reader]() -> std::optional<std::optional<RegionId>> {
        auto zoom = reader.Read<uint32_t>();
        auto x = reader.Read<int32_t>();
        auto y = reader.Read<int32_t>();

        if (!zoom || !x || !y || *zoom > 31)
        {
            return std::nullopt;
        }

        auto configuration = Configuration(static_cast<uint8_t>(*zoom));
        if (!configuration)
        {
            return std::optional<RegionId> {};
        }

        auto regions_per_side = (TilesAtZoom(configuration->zoom) + configuration->region_size - 1) /
                                configuration->region_size;
        if (*x < 0 || *y < 0 || *x >= regions_per_side || *y >= regions_per_side)
        {
            return std::nullopt;
        }

        return std::optional<RegionId> {RegionId {configuration->zoom, *x, *y}};
    };

    for (auto i = 0u; i < *pending_count; i++)
    {
        auto id = read_id();
        auto byte_count = reader.Read<uint32_t>();
        if (!id || !byte_count)
        {
            return false;
        }

        auto bytes = reader.ReadBytes(*byte_count);
        if (!bytes)
        {
            return false;
        }

        if (!*id)
        {
            // Unconfigured zoom, skip
            continue;
        }

        auto configuration = Configuration((*id)->zoom);
        if (*byte_count != BitmapBytes(configuration->region_size))
        {
            // Another region size, the bitmap doesn't fit
            continue;
        }

        if (std::ranges::find(pending, **id, &PendingRegion::id) != pending.end())
        {
            return false;
        }

        // Start from a new region, to get the parts outside the world right
        auto region = CreateRegion(**id, *configuration);
        for (auto index = 0u; index < TilesPerRegion(configuration->region_size); index++)
        {
            if (IsSet(region.on_disk, index))
            {
                continue;
            }

            if (std::to_integer<uint8_t>((*bytes)[index / 8]) & (1u << (index % 8)))
            {
                region.on_disk[index / 8] |= (1u << (index % 8));
                region.tiles_on_disk++;
            }
        }

        if (region.tiles_on_disk == TilesPerRegion(configuration->region_size))
        {
            completed.insert(region.id);
        }
        else
        {
            pending.push_back(std::move(region));
        }
    }

    for (auto i = 0u; i < *completed_count; i++)
    {
        auto id = read_id();
        if (!id)
        {
            return false;
        }
        if (*id)
        {
            completed.insert(**id);
        }
    }

    if (reader.Remaining() != 0)
    {
        return false;
    }

    // Completed has precedence (shouldn't be in both though)
    std::erase_if(pending, [&completed](const auto& region) { return completed.contains(region.id); });

    m_pending = std::move(pending);
    m_completed = std::move(completed);
    m_dirty = false;
    // The visited regions might not be there anymore
    std::ranges::fill(m_last_visited, std::nullopt);

    return true;
}
