#pragma once

#include "hal/i_gps.hh"
#include "wgs84_to_osm_point.hh"

struct GpsData
{
    GpsPosition position;
    Point pixel_position;

    float speed;
    float heading;

    // Add time, height, etc.
    bool operator==(const GpsData& other) const = default;
};
