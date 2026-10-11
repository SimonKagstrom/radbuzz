#pragma once

#include "wgs84_to_osm_point.hh"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

/**
 * Keeps track of which regions (square blocks of tiles) around visited places should be
 * downloaded, and which tiles of them are known to be on disk. Regions are kept as pending until
 * all their tiles are on disk, and then remembered as completed, so that revisiting them costs
 * nothing.
 *
 * No filesystem or network access, the user checks/fetches the tiles and reports back.
 */
class TileRegions
{
public:
    struct ZoomConfiguration
    {
        uint8_t zoom;
        // Width and height of a region, in tiles
        int32_t region_size;
        // Regions within this many tiles from a visited tile are marked as pending
        int32_t margin;
    };

    explicit TileRegions(std::span<const ZoomConfiguration> configurations);

    // A tile has been visited (i.e., the position is there). Tiles at unconfigured zooms are ignored
    void Visit(const Tile& tile);

    // The next tile not known to be on disk, most recently visited regions first. Each tile is
    // returned once per round, so nullopt when all have been returned (see RestartRound)
    std::optional<Tile> NextMissingTile();

    // The tile is on disk (e.g., downloaded, or found when checking)
    void TileOnDisk(const Tile& tile);

    // Return the tiles which are still missing (e.g., failed downloads) again
    void RestartRound();

    size_t PendingRegionCount() const;
    size_t CompletedRegionCount() const;

    // Changed since the last ClearDirty(), i.e., should be saved
    bool IsDirty() const;
    void ClearDirty();

    std::vector<std::byte> Serialize() const;

    // Replace the current regions. Returns false (and keeps the current state) for invalid data
    bool Deserialize(std::span<const std::byte> data);

private:
    struct RegionId
    {
        uint8_t zoom;
        int32_t x;
        int32_t y;

        bool operator==(const RegionId& other) const = default;
    };

    struct RegionIdHash
    {
        size_t operator()(const RegionId& id) const noexcept
        {
            return std::hash<Tile> {}(Tile {id.x, id.y, id.zoom});
        }
    };

    struct PendingRegion
    {
        RegionId id;
        // One bit per tile, row by row. Set if on disk
        std::vector<uint8_t> on_disk;
        uint32_t tiles_on_disk {0};
        // The next tile index to return this round
        uint32_t cursor {0};
    };

    const ZoomConfiguration* Configuration(uint8_t zoom) const;

    PendingRegion CreateRegion(const RegionId& id, const ZoomConfiguration& configuration) const;

    // Set the bit, and complete the region if it was the last. Returns true if completed
    bool SetOnDisk(size_t region_index, uint32_t tile_index);

    std::vector<ZoomConfiguration> m_configurations;
    // Per configuration, to skip the common case of visiting the same tile again
    std::vector<std::optional<Tile>> m_last_visited;

    // Most recently visited first
    std::vector<PendingRegion> m_pending;
    std::unordered_set<RegionId, RegionIdHash> m_completed;

    bool m_dirty {false};
};
