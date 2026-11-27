#pragma once

#include "hal/i_input.hh"

#include <cstdint>
#include <optional>

/**
 * Double taps in the screen corners, mapped to encoder events:
 *
 * - Top left: kButtonDown (the menu)
 * - Bottom left: kLeft (previous screen)
 * - Bottom right: kRight (next screen)
 *
 * Both taps must be in the same corner, but not at the same point.
 */
class DoubleTapDetector
{
public:
    static constexpr uint32_t kMaxIntervalMs = 500;

    DoubleTapDetector(int32_t display_width,
                      int32_t display_height,
                      int32_t corner_width,
                      int32_t corner_height)
        : m_display_width(display_width)
        , m_display_height(display_height)
        , m_corner_width(corner_width)
        , m_corner_height(corner_height)
    {
    }

    /**
     * Report a tap (short click).
     *
     * @return the event if this completes a double tap
     */
    std::optional<hal::IInput::EventType> OnTap(int32_t x, int32_t y, uint32_t now_ms)
    {
        const auto event = CornerEvent(x, y);

        if (event && event == m_last_event && now_ms - m_last_tap_ms <= kMaxIntervalMs)
        {
            // A third tap starts over
            m_last_event = std::nullopt;
            return event;
        }

        m_last_event = event;
        m_last_tap_ms = now_ms;

        return std::nullopt;
    }

private:
    std::optional<hal::IInput::EventType> CornerEvent(int32_t x, int32_t y) const
    {
        const auto left = x < m_corner_width;
        const auto right = x >= m_display_width - m_corner_width;
        const auto top = y < m_corner_height;
        const auto bottom = y >= m_display_height - m_corner_height;

        if (top && left)
        {
            return hal::IInput::EventType::kButtonDown;
        }
        if (bottom && left)
        {
            return hal::IInput::EventType::kLeft;
        }
        if (bottom && right)
        {
            return hal::IInput::EventType::kRight;
        }

        return std::nullopt;
    }

    const int32_t m_display_width;
    const int32_t m_display_height;
    const int32_t m_corner_width;
    const int32_t m_corner_height;

    std::optional<hal::IInput::EventType> m_last_event;
    uint32_t m_last_tap_ms {0};
};
