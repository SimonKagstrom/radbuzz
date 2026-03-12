#pragma once

#include "hal/i_gps.hh"

struct GpsData
{
    GpsPosition position;

    float speed;
    float heading;

    // Add time, height, etc.
    bool operator==(const GpsData& other) const = default;
};
