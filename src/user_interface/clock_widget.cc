#include "clock_widget.hh"

#include "clock_string.hh"

#include <radbuzz_font_22.h>

namespace
{

// Pushed above the screen, so that only the lower corners are rounded (like a tab)
constexpr int32_t kCornerRadius = 12;
constexpr int32_t kHorizontalPadding = 12;
constexpr int32_t kBottomPadding = 4;

// The time arrives from the phone or GPS, which the UI doesn't listen to
constexpr auto kInvalidRecheckTime = 5s;

} // namespace

ClockWidget::ClockWidget(lv_obj_t* parent)
    : m_box(lv_obj_create(parent))
    , m_label(lv_label_create(m_box))
{
    lv_obj_set_size(m_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(m_box, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(m_box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_box, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_radius(m_box, kCornerRadius, LV_PART_MAIN);
    lv_obj_set_style_pad_top(m_box, kCornerRadius, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(m_box, kBottomPadding, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(m_box, kHorizontalPadding, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(m_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(m_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(m_box, LV_ALIGN_TOP_MID, 0, -kCornerRadius);

    lv_obj_set_style_text_font(m_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_text(m_label, "");

    lv_obj_add_flag(m_box, LV_OBJ_FLAG_HIDDEN);
}

std::optional<milliseconds>
ClockWidget::Update(ApplicationState& state, bool show)
{
    if (!show || !state.Get<AS::configuration>()->show_clock)
    {
        lv_obj_add_flag(m_box, LV_OBJ_FLAG_HIDDEN);
        return std::nullopt;
    }

    // Not known yet
    const auto utc = state.Get<AS::clock>();
    if (utc == 0)
    {
        lv_obj_add_flag(m_box, LV_OBJ_FLAG_HIDDEN);
        return kInvalidRecheckTime;
    }

    // Only redraw when the minute changes
    if (auto text = ClockToString(utc, state.Get<AS::timezone>()); text != m_text)
    {
        m_text = std::move(text);
        lv_label_set_text(m_label, m_text.c_str());
    }
    lv_obj_remove_flag(m_box, LV_OBJ_FLAG_HIDDEN);

    return seconds(SecondsToNextMinute(utc));
}
