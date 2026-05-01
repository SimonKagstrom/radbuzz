#pragma once

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

struct ConfigurationSettings
{
    /// @brief Number of battery cells in series, used to calculate cell voltage from millivolts (14s3p etc)
    uint8_t battery_cell_series;
    /// @brief Maximum speed in km/h (for the speedometer limits)
    uint8_t max_speed;

    // Configuration from the filesystem
    WifiSsidData wifi_ssid_data;

    bool operator==(const ConfigurationSettings& other) const = default;
};
