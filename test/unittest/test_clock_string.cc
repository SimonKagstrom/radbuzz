#include "clock_string.hh"
#include "test.hh"

#include <limits>

TEST_SUITE_BEGIN("clock_string");

namespace
{

// 2026-10-24 12:34:56 UTC
constexpr uint32_t kNoon = 1792845296;

} // namespace

TEST_CASE("the clock is shown as local hours and minutes")
{
    CHECK(ClockToString(kNoon, 0) == "12:34");
    CHECK(ClockToString(kNoon, 120) == "14:34");
    CHECK(ClockToString(kNoon, -210) == "09:04");
    CHECK(ClockToString(kNoon, 345) == "18:19");
}

TEST_CASE("the clock wraps around midnight")
{
    // 2026-10-24 23:59:59 UTC
    constexpr uint32_t kBeforeMidnight = 1792886399;

    CHECK(ClockToString(kBeforeMidnight, 0) == "23:59");
    CHECK(ClockToString(kBeforeMidnight + 1, 0) == "00:00");
    CHECK(ClockToString(kBeforeMidnight, 60) == "00:59");
    CHECK(ClockToString(kBeforeMidnight, -12 * 60) == "11:59");
    CHECK(ClockToString(kBeforeMidnight + 1, -60) == "23:00");
}

TEST_CASE("the clock handles the extremes")
{
    CHECK(ClockToString(0, 0) == "00:00");
    CHECK(ClockToString(0, -60) == "23:00");
    CHECK(ClockToString(0, 14 * 60) == "14:00");
    // 2106-02-07 06:28:15 UTC
    CHECK(ClockToString(std::numeric_limits<uint32_t>::max(), 0) == "06:28");
    CHECK(ClockToString(std::numeric_limits<uint32_t>::max(), 14 * 60) == "20:28");
}

TEST_CASE("the time to the next minute is calculated")
{
    CHECK(SecondsToNextMinute(kNoon) == 4);
    CHECK(SecondsToNextMinute(kNoon + 4) == 60);
    CHECK(SecondsToNextMinute(kNoon + 3) == 1);
    CHECK(SecondsToNextMinute(0) == 60);
}

TEST_SUITE_END();
