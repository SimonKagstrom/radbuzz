#pragma once

#include "hal/i_gps.hh"

#include <cstdint>
#include <optional>
#include <unordered_map>

constexpr auto kTileSize = 256;

constexpr auto kDefaultZoom = 15;
constexpr auto kCityZoom = 13;
constexpr auto kLandscapeZoom = 10;

struct Tile
{
    int32_t x;
    int32_t y;
    uint8_t zoom;
};

namespace std
{
template <>
struct hash<Tile>
{
    std::size_t operator()(const Tile& t) const noexcept
    {
        auto h1 = std::hash<int32_t> {}(t.x);
        auto h2 = std::hash<int32_t> {}(t.y);
        auto h3 = std::hash<uint8_t> {}(t.zoom);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

} // namespace std

constexpr auto kInvalidTile = Tile {-1, -1, 0};

struct Point
{
     int32_t x;
    int32_t y;
    uint8_t zoom;
};

inline auto
ToPoint(const Tile& tile)
{
    return Point {tile.x * kTileSize, tile.y * kTileSize, tile.zoom};
}

inline auto
ToTile(const Point& point)
{
    return Tile {point.x / kTileSize, point.y / kTileSize, point.zoom};
}

inline bool
operator==(const Tile& lhs, const Tile& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.zoom == rhs.zoom;
}

inline bool
operator==(const Point& lhs, const Point& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.zoom == rhs.zoom;
}

auto operator<=>(const Tile& lhs, const Point& rhs) = delete;
auto operator<=>(const Point& lhs, const Tile& rhs) = delete;

std::optional<Point> Wgs84ToOsmPoint(const GpsPosition& position, uint8_t zoom);

GpsPosition OsmPointToWgs84(const Point& point);

Point OsmPointToPoint(const Point& point, uint8_t next_zoom);

float MetersPerPixelAtPoint(const Point& point);
