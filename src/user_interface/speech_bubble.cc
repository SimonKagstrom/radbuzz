#include "speech_bubble.hh"

SpeechBubble::SpeechBubble(lv_obj_t* pointing_at, Direction direction, const char* text)
    : m_pointing_at(pointing_at)
{
    m_bubble = lv_obj_create(lv_layer_top());

    lv_obj_set_style_bg_color(m_bubble, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_bubble, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_bubble, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_bubble, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(m_bubble, 8, LV_PART_MAIN);
    lv_obj_set_width(m_bubble, LV_SIZE_CONTENT);
    lv_obj_set_height(m_bubble, LV_SIZE_CONTENT);

    m_text_label = lv_label_create(m_bubble);
    lv_label_set_text(m_text_label, text);
    lv_obj_set_style_text_color(m_text_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_text_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(m_text_label, &radbuzz_font_16, LV_PART_MAIN);

    lv_obj_center(m_text_label);

    switch (direction)
    {
    case Direction::kAbove:
        lv_obj_align_to(m_bubble, pointing_at, LV_ALIGN_OUT_TOP_MID, 0, -4);
        break;
    case Direction::kBelow:
        lv_obj_align_to(m_bubble, pointing_at, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
        break;
    case Direction::kLeft:
        lv_obj_align_to(m_bubble, pointing_at, LV_ALIGN_OUT_LEFT_MID, 4, 0);
        break;
    case Direction::kRight:
        lv_obj_align_to(m_bubble, pointing_at, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        break;
    }

    auto tail = lv_canvas_create(lv_layer_top());
    lv_canvas_set_buffer(tail,
                         static_cast<void*>(m_tail_canvas_buffer),
                         kTailWidth,
                         kTailHeight,
                         LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(tail, lv_color_black(), LV_OPA_TRANSP);

    lv_layer_t layer;
    lv_canvas_init_layer(tail, &layer);
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = lv_obj_get_style_bg_color(m_bubble, LV_PART_MAIN);

    switch (direction)
    {
    case Direction::kAbove:
        tri_dsc.p[0] = {0, 0};
        tri_dsc.p[1] = {kTailWidth - 1, 0};
        tri_dsc.p[2] = {kTailWidth / 2, kTailHeight - 1};
        break;
    case Direction::kBelow:
        tri_dsc.p[0] = {kTailWidth / 2, 0};
        tri_dsc.p[1] = {0, kTailHeight - 1};
        tri_dsc.p[2] = {kTailWidth - 1, kTailHeight - 1};
        break;
    case Direction::kLeft:
        tri_dsc.p[0] = {0, 0};
        tri_dsc.p[1] = {0, kTailHeight - 1};
        tri_dsc.p[2] = {kTailWidth - 1, kTailHeight / 2};
        break;
    case Direction::kRight:
        tri_dsc.p[0] = {kTailWidth - 1, 0};
        tri_dsc.p[1] = {kTailWidth - 1, kTailHeight - 1};
        tri_dsc.p[2] = {0, kTailHeight / 2};
        break;
    }

    lv_draw_triangle(&layer, &tri_dsc);
    lv_canvas_finish_layer(tail, &layer);

    m_bubble_tail = tail;
    switch (direction)
    {
    case Direction::kAbove:
        lv_obj_align_to(m_bubble_tail, m_bubble, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        break;
    case Direction::kBelow:
        lv_obj_align_to(m_bubble_tail, m_bubble, LV_ALIGN_OUT_TOP_MID, 0, 0);
        break;
    case Direction::kLeft:
        lv_obj_align_to(m_bubble_tail, m_bubble, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
        break;
    case Direction::kRight:
        lv_obj_align_to(m_bubble_tail, m_bubble, LV_ALIGN_OUT_LEFT_MID, 0, 0);
        break;
    }

    lv_obj_move_foreground(m_bubble_tail);
    lv_obj_move_foreground(m_bubble);
}

SpeechBubble::~SpeechBubble()
{
    lv_obj_del(m_text_label);
    lv_obj_del(m_bubble_tail);
    lv_obj_del(m_bubble);
}

void
SpeechBubble::Update()
{
    auto hidden = !lv_obj_is_visible(m_pointing_at);

    lv_obj_set_flag(m_bubble, LV_OBJ_FLAG_HIDDEN, hidden);
    lv_obj_set_flag(m_bubble_tail, LV_OBJ_FLAG_HIDDEN, hidden);
}
