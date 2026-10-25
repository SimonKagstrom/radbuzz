#include "tile_styles.hh"

#include <format>

namespace
{

// The zoom levels which have been downloaded before the style directory existed
constexpr auto kUnstyledZooms = std::array {kDefaultZoom, kCityZoom, kLandscapeZoom};

constexpr auto kUnstyledRegionsPath = "pending/regions.bin";

} // namespace

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
