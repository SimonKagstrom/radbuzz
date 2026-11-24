#pragma once

#include "hal/i_gps.hh"
#include "wgs84_to_osm_point.hh"

#include <cstdint>
#include <string>
#include <vector>

struct WifiSsidNetwork
{
    std::string ssid;
    std::string password;

    bool operator==(const WifiSsidNetwork& other) const = default;
};

struct WifiSsidData
{
    std::vector<WifiSsidNetwork> networks;

    bool operator==(const WifiSsidData& other) const = default;
};

/// Where to upload trips, from traccar.txt. Not configured when the id is empty
struct TraccarSettings
{
    /// The server, e.g., https://host.tailnet.ts.net
    std::string url;
    /// The Traccar device identifier for real trips
    std::string id;
    /// The Traccar device identifier for demo trips, which aren't uploaded when empty
    std::string demo_id;

    bool operator==(const TraccarSettings& other) const = default;
};

enum class SpeedometerType : uint8_t
{
    kAnalog,
    kDigital,
    kBoth,

    kValueCount,
};

enum class HistogramMode : uint8_t
{
    kPower,
    kConsumption,

    kValueCount,
};

// The map tile styles, see tile_styles.hh
enum class TileStyle : uint8_t
{
    kCycle,
    kAtlas,
    kMobileAtlas,
    kNeighbourhood,
    kLandscape,

    kValueCount,
};

enum class Profile : uint8_t
{
    kWalking,
    kMoped25,
    kMoped30,
    kMoped45,
    kNoLimit,

    kValueCount,
};

// The home position, or Stockholm when it has not been set
inline GpsPosition
HomePositionOrDefault(const Point& home_position)
{
    if (home_position.x == 0 && home_position.y == 0)
    {
        return GpsPosition {59.3293f, 18.0686f};
    }

    return OsmPointToWgs84(home_position);
}
