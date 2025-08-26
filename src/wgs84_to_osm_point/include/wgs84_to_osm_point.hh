#pragma once

#include "hal/i_gps.hh"

#include <cstdint>
#include <optional>

constexpr auto kTileSize = 256;
constexpr auto kCityTileFactor = 15;

struct Tile
{
    int32_t x;
    int32_t y;
};

constexpr auto kInvalidTile = Tile {-1, -1};

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

inline auto
ToCityTile(const Point& point)
{
    return Tile {(point.x / kTileSize / kCityTileFactor) * kCityTileFactor,
                 (point.y / kTileSize / kCityTileFactor) * kCityTileFactor};
}

inline bool
operator==(const Tile& lhs, const Tile& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool
operator==(const Point& lhs, const Point& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

auto operator<=>(const Tile& lhs, const Point& rhs) = delete;
auto operator<=>(const Point& lhs, const Tile& rhs) = delete;

std::optional<Point> Wgs84ToOsmPoint(GpsPosition position, int zoom);

GpsPosition OsmPointToWgs84(Point point, int zoom);
