#pragma once

#include "hal/i_gps.hh"

#include <cstdint>
#include <etl/unordered_set.h>
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
        const uint32_t x = static_cast<uint32_t>(t.x);
        const uint32_t y = static_cast<uint32_t>(t.y);

        // Optimized for 32-bit size_t targets (ESP32): MurmurHash3 finalizer with zoom in low 4 bits.
        uint32_t h = 0x811C9DC5u;
        h ^= x * 0x85EBCA6Bu;
        h = (h << 13) | (h >> 19);
        h ^= y * 0xC2B2AE35u;
        h ^= h >> 16;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13;
        h *= 0xC2B2AE35u;
        h ^= h >> 16;

        // Reserve low 4 bits for zoom (10..15 in practice).
        h = (h & ~uint32_t {0x0F}) | (static_cast<uint32_t>(t.zoom) & 0x0F);
        return static_cast<std::size_t>(h);
    }
};

} // namespace std

namespace etl
{

template <>
struct hash<Tile>
{
    std::size_t operator()(const Tile& t) const noexcept
    {
        return std::hash<Tile> {}(t);
    }
};

} // namespace etl

constexpr auto kInvalidTile = Tile {std::numeric_limits<decltype(Tile::x)>::max(),
                                    std::numeric_limits<decltype(Tile::y)>::max(),
                                    0};

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
    return Tile {static_cast<decltype(Tile::x)>(point.x / kTileSize),
                 static_cast<decltype(Tile::y)>(point.y / kTileSize),
                 point.zoom};
}

inline auto
TileId(const Tile &tile)
{
    // Lowermost two bits the unique value of 10,13,15 zooms
    uint32_t out = (tile.zoom >> 1) & 0b11;

    // At zoom 15, the global maximum tile value is 32767, which fits in 15 bits, so x and y
    // can be safely packed in the remaining 30 bits.
    out |= (static_cast<uint32_t>(tile.x) << 17);
    out |= static_cast<uint32_t>(tile.y) << 2;

    return out;
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

uint32_t MetersBetweenPoints(const Point& p1, const Point& p2);
