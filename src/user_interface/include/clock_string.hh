#pragma once

#include <cstdint>
#include <format>
#include <string>

// The local time ("HH:MM") from the UTC clock (seconds since the epoch) and the timezone offset
inline std::string
ClockToString(uint32_t utc, int16_t timezone_minutes)
{
    constexpr int64_t kSecondsPerDay = 24 * 60 * 60;

    // 64 bits, since the offset can push it outside of the uint32_t range
    const auto local = static_cast<int64_t>(utc) + timezone_minutes * 60;
    const auto seconds_of_day = ((local % kSecondsPerDay) + kSecondsPerDay) % kSecondsPerDay;

    return std::format("{:02}:{:02}", seconds_of_day / 3600, (seconds_of_day % 3600) / 60);
}

// Until the minute changes. Timezones are whole minutes, so the same for UTC and local time
inline uint32_t
SecondsToNextMinute(uint32_t utc)
{
    return 60 - utc % 60;
}
