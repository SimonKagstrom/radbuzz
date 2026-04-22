#pragma once

#include <cstdint>

struct ConfigurationSettings
{
    /// @brief Number of battery cells in series, used to calculate cell voltage from millivolts (14s3p etc)
    uint8_t battery_cell_series;
    /// @brief Maximum speed in km/h (for the speedometer limits)
    uint8_t max_speed;

    bool operator==(const ConfigurationSettings& other) const = default;
};
