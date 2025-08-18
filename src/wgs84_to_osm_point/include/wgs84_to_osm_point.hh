#pragma once

#include "hal/i_gps.hh"

#include <cstdint>
#include <optional>

constexpr auto kTileSize = 256;

struct Tile
{
    int32_t x;
    int32_t y;
};

struct Point
{
    int32_t x;
    int32_t y;
};

inline auto
ToPoint(const Tile& tile)
{
    return Point {tile.x * kTileSize, tile.y * kTileSize};
}

inline auto
ToTile(const Point& point)
{
    return Tile {point.x / kTileSize, point.y / kTileSize};
}

std::optional<Point> Wgs84ToOsmPoint(GpsPosition position, int zoom);
