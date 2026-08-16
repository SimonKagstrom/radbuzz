#pragma once

#include <format>
#include <string>

static inline std::string
SecondsToString(uint32_t seconds)
{
    if (seconds >= 3600)
    {
        // Hours
        return std::format(
            "{:02}:{:02}:{:02}", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
    }
    else
    {
        // Seconds / minutes
        return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
    }
}

static inline std::string
SecondsToString(seconds seconds)
{
    return SecondsToString(seconds.count());
}
