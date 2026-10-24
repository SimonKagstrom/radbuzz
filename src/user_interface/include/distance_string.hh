#pragma once

#include <cstdint>
#include <format>
#include <string>

struct DistanceString
{
    std::string value;
    std::string unit;
};

// Meters below 1 km, otherwise km with one decimal (rounded)
inline DistanceString
DistanceToValueAndUnit(uint32_t meters)
{
    if (meters < 1000)
    {
        return {std::format("{}", meters), "m"};
    }

    // Integer math to avoid float rounding surprises
    auto tenths = (static_cast<uint64_t>(meters) + 50) / 100;
    return {std::format("{}.{}", tenths / 10, tenths % 10), "km"};
}

// E.g., "300 m" or "1.4 km"
inline std::string
DistanceToString(uint32_t meters)
{
    auto [value, unit] = DistanceToValueAndUnit(meters);

    return std::format("{} {}", value, unit);
}
