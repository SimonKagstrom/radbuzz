#include "tile_styles.hh"

#include "tile_style_atlas.hh"
#include "tile_style_cycle.hh"
#include "tile_style_landscape.hh"
#include "tile_style_mobile_atlas.hh"
#include "tile_style_neighbourhood.hh"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace
{

struct TileStyleInfo
{
    std::string_view name;
    // In the URL, https://tile.thunderforest.com/<url_name>/...
    std::string_view url_name;
    // On disk. At most 8 characters, since long filenames are not enabled for FAT
    std::string_view directory;
    // PNG
    std::span<const uint8_t> sample;
};

constexpr auto kTileStyles = std::to_array<TileStyleInfo>({
    {.name = "OpenCycleMap",
     .url_name = "cycle",
     .directory = "cycle",
     .sample = tile_style_cycle_data},
    {.name = "Atlas", .url_name = "atlas", .directory = "atlas", .sample = tile_style_atlas_data},
    {.name = "Atlas mobile (high contrast)",
     .url_name = "mobile-atlas",
     .directory = "mobatlas",
     .sample = tile_style_mobile_atlas_data},
    {.name = "Neighbourhood",
     .url_name = "neighbourhood",
     .directory = "neighbor",
     .sample = tile_style_neighbourhood_data},
    {.name = "Landscape",
     .url_name = "landscape",
     .directory = "landscap",
     .sample = tile_style_landscape_data},
});
static_assert(kTileStyles.size() == static_cast<size_t>(TileStyle::kValueCount));
static_assert(std::ranges::all_of(kTileStyles, [](const auto& info) {
    return !info.directory.empty() && info.directory.size() <= 8;
}));

consteval bool
DirectoriesAreUnique()
{
    auto directories = std::array<std::string_view, kTileStyles.size()> {};
    std::ranges::transform(kTileStyles, directories.begin(), &TileStyleInfo::directory);

    std::ranges::sort(directories);
    return std::ranges::adjacent_find(directories) == directories.end();
}
static_assert(DirectoriesAreUnique(), "Styles must not share tiles on disk");

// The zoom levels which have been downloaded before the style directory existed
constexpr auto kUnstyledZooms = std::array {kDefaultZoom, kCityZoom, kLandscapeZoom};

constexpr auto kUnstyledRegionsPath = "pending/regions.bin";

constexpr const TileStyleInfo&
GetTileStyleInfo(TileStyle style)
{
    return kTileStyles[std::to_underlying(style)];
}

} // namespace

std::string_view
TileStyleName(TileStyle style)
{
    return GetTileStyleInfo(style).name;
}

std::span<const std::byte>
TileStyleSample(TileStyle style)
{
    return std::as_bytes(GetTileStyleInfo(style).sample);
}

std::string
TileUrl(TileStyle style, const Tile& tile, std::string_view api_key)
{
    return std::format("https://tile.thunderforest.com/{}/{}/{}/{}.png?apikey={}",
                       GetTileStyleInfo(style).url_name,
                       tile.zoom,
                       tile.x,
                       tile.y,
                       api_key);
}

std::string
TilePath(TileStyle style, const Tile& tile)
{
    return std::format(
        "tiles/{}/{}/{}/{}.png", GetTileStyleInfo(style).directory, tile.zoom, tile.x, tile.y);
}

std::string
RegionsPath(TileStyle style)
{
    return std::format("pending/{}/regions.bin", GetTileStyleInfo(style).directory);
}

void
MigrateUnstyledTiles(const Filesystem& filesystem)
{
    const auto& cycle = GetTileStyleInfo(TileStyle::kCycle);

    // Each one fails if not there (i.e., already moved), or if the destination already exists
    for (auto zoom : kUnstyledZooms)
    {
        filesystem.Move(std::format("tiles/{}", zoom),
                        std::format("tiles/{}/{}", cycle.directory, zoom));
    }
    filesystem.Move(kUnstyledRegionsPath, RegionsPath(TileStyle::kCycle));
}
