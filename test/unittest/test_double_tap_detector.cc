#include "double_tap_detector.hh"
#include "test.hh"

TEST_SUITE_BEGIN("double_tap_detector");

namespace
{

using EventType = hal::IInput::EventType;

constexpr int32_t kWidth = 800;
constexpr int32_t kHeight = 480;
constexpr int32_t kCornerWidth = 128;
constexpr int32_t kCornerHeight = 96;
constexpr int32_t kRight = kWidth - 10;
constexpr int32_t kBottom = kHeight - 10;

} // namespace

SCENARIO("double taps in the corners are mapped to encoder events")
{
    GIVEN("a double tap detector")
    {
        DoubleTapDetector detector(kWidth, kHeight, kCornerWidth, kCornerHeight);

        WHEN("double tapping in the top left corner")
        {
            THEN("the first tap is ignored, and the second gives a button press")
            {
                CHECK(detector.OnTap(10, 10, 1000) == std::nullopt);
                CHECK(detector.OnTap(40, 30, 1200) == EventType::kButtonDown);
            }
        }

        WHEN("double tapping in the lower left corner")
        {
            CHECK(detector.OnTap(50, kBottom, 1000) == std::nullopt);

            THEN("it's a left turn")
            {
                CHECK(detector.OnTap(70, kBottom - 20, 1300) == EventType::kLeft);
            }
        }

        WHEN("double tapping in the lower right corner")
        {
            CHECK(detector.OnTap(kRight, kBottom, 1000) == std::nullopt);

            THEN("it's a right turn")
            {
                CHECK(detector.OnTap(kRight - 20, kBottom, 1300) == EventType::kRight);
            }
        }

        WHEN("double tapping in the top right corner")
        {
            THEN("nothing happens")
            {
                CHECK(detector.OnTap(kRight, 10, 1000) == std::nullopt);
                CHECK(detector.OnTap(kRight, 10, 1100) == std::nullopt);
            }
        }

        WHEN("double tapping in the middle of the screen")
        {
            THEN("nothing happens")
            {
                CHECK(detector.OnTap(kWidth / 2, kHeight / 2, 1000) == std::nullopt);
                CHECK(detector.OnTap(kWidth / 2, kHeight / 2, 1100) == std::nullopt);
            }
        }

        WHEN("double tapping at the top or bottom, but outside the corners")
        {
            THEN("nothing happens")
            {
                CHECK(detector.OnTap(kWidth / 2, 10, 1000) == std::nullopt);
                CHECK(detector.OnTap(kWidth / 2, 10, 1100) == std::nullopt);
                CHECK(detector.OnTap(kCornerWidth, kBottom, 1200) == std::nullopt);
                CHECK(detector.OnTap(kCornerWidth, kBottom, 1300) == std::nullopt);
                CHECK(detector.OnTap(10, kCornerHeight, 1400) == std::nullopt);
                CHECK(detector.OnTap(10, kCornerHeight, 1500) == std::nullopt);
            }
        }
    }
}

SCENARIO("taps which are not double taps")
{
    GIVEN("a double tap detector")
    {
        DoubleTapDetector detector(kWidth, kHeight, kCornerWidth, kCornerHeight);

        WHEN("the second tap comes too late")
        {
            CHECK(detector.OnTap(50, kBottom, 1000) == std::nullopt);
            CHECK(detector.OnTap(50, kBottom, 1000 + DoubleTapDetector::kMaxIntervalMs + 1) ==
                  std::nullopt);

            THEN("a third tap in time with the second is a double tap")
            {
                CHECK(detector.OnTap(50, kBottom, 1700) == EventType::kLeft);
            }
        }

        WHEN("the taps are in different corners")
        {
            THEN("nothing happens")
            {
                CHECK(detector.OnTap(50, kBottom, 1000) == std::nullopt);
                CHECK(detector.OnTap(kRight, kBottom, 1100) == std::nullopt);
                CHECK(detector.OnTap(10, 10, 1200) == std::nullopt);
            }
        }

        WHEN("a tap outside the corners comes in between")
        {
            THEN("nothing happens")
            {
                CHECK(detector.OnTap(50, kBottom, 1000) == std::nullopt);
                CHECK(detector.OnTap(kWidth / 2, kHeight / 2, 1100) == std::nullopt);
                CHECK(detector.OnTap(50, kBottom, 1200) == std::nullopt);
            }
        }

        WHEN("triple tapping")
        {
            CHECK(detector.OnTap(50, kBottom, 1000) == std::nullopt);
            CHECK(detector.OnTap(50, kBottom, 1100) == EventType::kLeft);

            THEN("the third tap starts a new double tap")
            {
                CHECK(detector.OnTap(50, kBottom, 1200) == std::nullopt);
                CHECK(detector.OnTap(50, kBottom, 1300) == EventType::kLeft);
            }
        }
    }
}

TEST_SUITE_END();
