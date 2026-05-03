#include "wgs84_to_osm_point.hh"

#include <cmath>
#include <numbers>
#include <optional>

namespace
{

constexpr float kEarthCircumferenceMeters = 40075016.686f;

inline float
deg2rad(float deg)
{
    return deg * (std::numbers::pi_v<float> / 180.0f);
}

} // namespace

std::optional<Point>
Wgs84ToOsmPoint(const GpsPosition& position, uint8_t zoom)
{
    float lat_rad = deg2rad(position.latitude);
    float n = std::powf(2.0f, zoom);

    float x = (position.longitude + 180.0f) / 360.0f * n;
    float y = (1.0f - std::asinhf(std::tanf(lat_rad)) / std::numbers::pi_v<float>) / 2.0f * n;

    return Point {static_cast<int32_t>(x * kTileSize), static_cast<int32_t>(y * kTileSize), zoom};
}

Point
OsmPointToPoint(const Point& point, uint8_t next_zoom)
{
    float scale = std::powf(2.0f, static_cast<int>(next_zoom) - static_cast<int>(point.zoom));
    return Point {
        static_cast<int32_t>(point.x * scale), static_cast<int32_t>(point.y * scale), next_zoom};
}

float
MetersPerPixelAtZoom(const Point& point)
{
    const auto pos = OsmPointToWgs84(point);
    const float n = std::powf(2.0f, point.zoom);
    const float lat_rad = deg2rad(pos.latitude);
    const float meters_per_pixel =
        (std::cosf(lat_rad) * kEarthCircumferenceMeters) / (kTileSize * n);
    return std::max(meters_per_pixel, 0.001f);
}

GpsPosition
OsmPointToWgs84(const Point& point)
{
    float n = std::powf(2.0f, point.zoom);

    float x = static_cast<float>(point.x) / kTileSize;
    float y = static_cast<float>(point.y) / kTileSize;

    float lon = x / n * 360.0f - 180.0f;
    float lat_rad = std::atan(std::sinh(std::numbers::pi_v<float> * (1.0f - 2.0f * y / n)));
    float lat = lat_rad * (180.0f / std::numbers::pi_v<float>);

    return GpsPosition {lat, lon};
}
