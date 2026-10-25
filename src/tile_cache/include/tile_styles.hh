#pragma once

#include "filesystem.hh"
#include "wgs84_to_osm_point.hh"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

// The map styles, all from Thunderforest
enum class TileStyle : uint8_t
{
    kCycle,

    kValueCount,
};

struct TileStyleInfo
{
    // Shown to the user
    std::string_view name;
    // In the URL, https://tile.thunderforest.com/<url_name>/...
    std::string_view url_name;
    // On disk. At most 8 characters, since long filenames are not enabled for FAT
    std::string_view directory;
};

constexpr auto kTileStyles = std::to_array<TileStyleInfo>({
    {.name = "OpenCycleMap", .url_name = "cycle", .directory = "cycle"},
});
static_assert(kTileStyles.size() == static_cast<size_t>(TileStyle::kValueCount));
static_assert(std::ranges::all_of(kTileStyles, [](const auto& info) {
    return !info.directory.empty() && info.directory.size() <= 8;
}));

constexpr const TileStyleInfo&
GetTileStyleInfo(TileStyle style)
{
    return kTileStyles[std::to_underlying(style)];
}

// tiles/<style>/<zoom>/<x>/<y>.png
std::string TilePath(TileStyle style, const Tile& tile);

// pending/<style>/regions.bin
std::string RegionsPath(TileStyle style);

// Tiles and regions used to be stored without the style directory, and were all OpenCycleMap
// tiles. Move them into the cycle directory. Does nothing if already done
void MigrateUnstyledTiles(const Filesystem& filesystem);
