#pragma once

#include "hal/i_gps.hh"

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

enum class SpeedometerType : uint8_t
{
    kAnalog,
    kDigital,
    kBoth,

    kValueCount,
};

struct ConfigurationSettings
{
    // @brief the home position
    GpsPosition home_position;
    // Configuration from the filesystem
    WifiSsidData wifi_ssid_data;
    /// @brief Maximum power in watts
    uint16_t max_watts;

    SpeedometerType speedometer_type;

    /// Whether the map should rotate according to heading
    bool rotate_map;
    /// @brief Number of battery cells in series, used to calculate cell voltage from millivolts (14s3p etc)
    uint8_t battery_cell_series;
    /// @brief Maximum speed in km/h (for the speedometer limits)
    uint8_t max_speed;
    /// @brief Battery capacity in ampere-hours (e.g., 20Ah)
    uint8_t battery_amp_hours;
    /// @brief Average watt-hours per kilometer for range estimation
    uint8_t wh_per_km_for_range_estimation;

    bool operator==(const ConfigurationSettings& other) const = default;
};
