#include "distance_string.hh"
#include "test.hh"

#include <limits>

TEST_SUITE_BEGIN("distance_string");

TEST_CASE("distances below 1 km are shown in meters")
{
    CHECK(DistanceToString(0) == "0 m");
    CHECK(DistanceToString(1) == "1 m");
    CHECK(DistanceToString(300) == "300 m");
    CHECK(DistanceToString(999) == "999 m");
}

TEST_CASE("distances from 1 km are shown in km with one decimal")
{
    CHECK(DistanceToString(1000) == "1.0 km");
    CHECK(DistanceToString(1400) == "1.4 km");
    CHECK(DistanceToString(12000) == "12.0 km");
    CHECK(DistanceToString(123456) == "123.5 km");

    WHEN("the distance is between tenths")
    {
        THEN("it's rounded to the nearest")
        {
            CHECK(DistanceToString(1049) == "1.0 km");
            CHECK(DistanceToString(1050) == "1.1 km");
            CHECK(DistanceToString(1949) == "1.9 km");
            CHECK(DistanceToString(1950) == "2.0 km");
            CHECK(DistanceToString(9999) == "10.0 km");
        }
    }

    WHEN("the distance is huge")
    {
        THEN("it doesn't overflow")
        {
            CHECK(DistanceToString(std::numeric_limits<uint32_t>::max()) == "4294967.3 km");
        }
    }
}

TEST_CASE("the value and unit can be retrieved separately")
{
    auto [m_value, m_unit] = DistanceToValueAndUnit(300);
    CHECK(m_value == "300");
    CHECK(m_unit == "m");

    auto [km_value, km_unit] = DistanceToValueAndUnit(1400);
    CHECK(km_value == "1.4");
    CHECK(km_unit == "km");
}
