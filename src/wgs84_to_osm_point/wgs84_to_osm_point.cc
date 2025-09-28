#include "wgs84_to_osm_point.hh"

#include <cmath>
#include <numbers>
#include <optional>

namespace
{

inline float
deg2rad(float deg)
{
    return deg * (std::numbers::pi_v<float> / 180.0f);
}

} // namespace

std::optional<Point>
Wgs84ToOsmPoint(GpsPosition position, int zoom)
{
    float lat_rad = deg2rad(position.latitude);
    float n = std::powf(2.0f, zoom);

    float x = (position.longitude + 180.0f) / 360.0f * n;
    float y = (1.0f - std::asinhf(std::tanf(lat_rad)) / std::numbers::pi_v<float>) / 2.0f * n;

    return Point {static_cast<int32_t>(x * kTileSize), static_cast<int32_t>(y * kTileSize)};
}

GpsPosition
OsmPointToWgs84(Point point, int zoom)
{
    float n = std::powf(2.0f, zoom);

    float x = static_cast<float>(point.x) / kTileSize;
    float y = static_cast<float>(point.y) / kTileSize;

    float lon = x / n * 360.0f - 180.0f;
    float lat_rad = std::atan(std::sinh(std::numbers::pi_v<float> * (1.0f - 2.0f * y / n)));
    float lat = lat_rad * (180.0f / std::numbers::pi_v<float>);

    return GpsPosition {lat, lon};
}
