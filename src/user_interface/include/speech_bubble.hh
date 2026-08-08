#pragma once

#include "radbuzz_font_16.h"

#include <lvgl.h>

class SpeechBubble final
{
public:
    enum class Direction
    {
        kAbove,
        kBelow,
        kLeft,
        kRight,
    };

    SpeechBubble(lv_obj_t* screen, lv_obj_t* pointing_at, Direction direction, const char* text);

    ~SpeechBubble();

    void Update();

private:
    static constexpr lv_coord_t kTailWidth = 16;
    static constexpr lv_coord_t kTailHeight = 12;

    lv_obj_t* m_pointing_at {nullptr};
    lv_obj_t* m_text_label {nullptr};
    lv_obj_t* m_bubble {nullptr};
    lv_obj_t* m_bubble_tail {nullptr};
    uint8_t m_tail_canvas_buffer[kTailWidth * kTailHeight * 4] = {};
};