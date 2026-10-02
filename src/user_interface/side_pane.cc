#include "side_pane.hh"

#include "lv_event_listener.hh"

#include <algorithm>
#include <radbuzz_font_22.h>

namespace
{

// Pushed outside the left edge of the screen, so that only the right corners are rounded
constexpr int32_t kCornerRadius = 0;

constexpr int32_t kTopMargin = 44;

constexpr int32_t kPadding = 12;

constexpr int32_t kButtonWidth = 90;
constexpr int32_t kButtonHeight = 45;
constexpr int32_t kCounterWidth = 60;
constexpr int32_t kCounterGap = 12;

// Fully opaque, or e.g., LV_OPA_80 to see the screen below
constexpr lv_opa_t kBackgroundOpacity = LV_OPA_COVER;

// Two lines per encoder step
constexpr int32_t kLineHeight = 26;
constexpr int32_t kScrollStep = 2 * kLineHeight;

void
MakeTransparent(lv_obj_t* obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

lv_obj_t*
CreateLabel(lv_obj_t* parent)
{
    auto label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(label, "");

    return label;
}

} // namespace

SidePane::SidePane(lv_obj_t* parent)
    : m_pane(lv_obj_create(parent))
{
    lv_obj_set_size(m_pane, kWidth + kCornerRadius, hal::kDisplayHeight);
    lv_obj_align(m_pane, LV_ALIGN_TOP_LEFT, -kCornerRadius, 0);
    lv_obj_set_style_bg_color(m_pane, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_pane, kBackgroundOpacity, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_pane, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_pane, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_pane, kCornerRadius, LV_PART_MAIN);
    // The part outside the screen + the visible padding
    lv_obj_set_style_pad_left(m_pane, kCornerRadius + kPadding, LV_PART_MAIN);
    lv_obj_set_style_pad_right(m_pane, kPadding, LV_PART_MAIN);
    lv_obj_set_style_pad_top(m_pane, kTopMargin, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(m_pane, kPadding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(m_pane, kPadding, LV_PART_MAIN);
    lv_obj_clear_flag(m_pane, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(m_pane, LV_FLEX_FLOW_COLUMN);

    // <source>: <title>, on one line
    m_title_label = CreateLabel(m_pane);
    // A fixed height, since the label would otherwise grow instead of cutting the text
    lv_obj_set_size(m_title_label, LV_PCT(100), lv_font_get_line_height(&radbuzz_font_22));
    lv_obj_set_style_text_align(m_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(m_title_label, LV_LABEL_LONG_DOT);

    // The message, scrollable in the remaining space
    m_body = lv_obj_create(m_pane);
    MakeTransparent(m_body);
    lv_obj_set_width(m_body, LV_PCT(100));
    lv_obj_set_flex_grow(m_body, 1);
    lv_obj_set_scroll_dir(m_body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_body, LV_SCROLLBAR_MODE_OFF);
    // Both for the encoder and touch drags
    LvEventListener::Create(m_body, LV_EVENT_SCROLL, [this](lv_event_t*) { UpdateOkButton(); });

    m_body_label = CreateLabel(m_body);
    lv_obj_set_width(m_body_label, LV_PCT(100));
    lv_label_set_long_mode(m_body_label, LV_LABEL_LONG_WRAP);

    // OK centered, with the counter to the right of it
    auto bottom_row = lv_obj_create(m_pane);
    MakeTransparent(bottom_row);
    lv_obj_set_size(bottom_row, LV_PCT(100), kButtonHeight);
    lv_obj_clear_flag(bottom_row, LV_OBJ_FLAG_SCROLLABLE);

    m_ok_button = lv_button_create(bottom_row);
    lv_obj_set_size(m_ok_button, kButtonWidth, kButtonHeight);
    lv_obj_set_style_radius(m_ok_button, 8, LV_PART_MAIN);
    lv_obj_align(m_ok_button, LV_ALIGN_CENTER, 0, 0);

    auto ok_label = CreateLabel(m_ok_button);
    lv_label_set_text(ok_label, "OK");
    lv_obj_center(ok_label);

    // Touch always dismisses, also before the end has been reached
    LvEventListener::Create(m_ok_button, LV_EVENT_CLICKED, [this](lv_event_t*) { Dismiss(); });

    m_counter_label = CreateLabel(bottom_row);
    lv_obj_set_width(m_counter_label, kCounterWidth);
    lv_obj_align(
        m_counter_label, LV_ALIGN_CENTER, kButtonWidth / 2 + kCounterGap + kCounterWidth / 2, 0);

    UpdateVisibility();
}

SidePane::~SidePane()
{
    lv_obj_delete(m_pane);
}

void
SidePane::AddMessage(const Message& message)
{
    const auto title =
        message.source.empty() ? message.title : message.source + ": " + message.title;

    lv_label_set_text(m_title_label, title.c_str());
    lv_label_set_text(m_body_label, message.body.c_str());
    lv_label_set_text(m_counter_label, "1/1");
    lv_obj_scroll_to_y(m_body, 0, LV_ANIM_OFF);

    m_has_message = true;
    UpdateVisibility();

    // The text size is needed to know if it fits
    lv_obj_update_layout(m_pane);
    UpdateOkButton();
}

void
SidePane::Dismiss()
{
    m_has_message = false;
    UpdateVisibility();
}

bool
SidePane::IsShown() const
{
    return m_has_message && !m_suppressed;
}

void
SidePane::SetSuppressed(bool suppressed)
{
    m_suppressed = suppressed;
    UpdateVisibility();
}

bool
SidePane::Contains(int32_t x, int32_t y) const
{
    lv_area_t area;

    lv_obj_get_coords(m_pane, &area);

    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}

void
SidePane::HandleInput(const Input::Event& event)
{
    switch (event.type)
    {
    case hal::IInput::EventType::kLeft:
        Scroll(-kScrollStep);
        break;
    case hal::IInput::EventType::kRight:
        Scroll(kScrollStep);
        break;
    case hal::IInput::EventType::kButtonUp:
        // On release, so that the press doesn't reach the screen below
        if (AtBottom())
        {
            Dismiss();
        }
        else
        {
            // A page, but keep the last line visible
            Scroll(std::max(lv_obj_get_content_height(m_body) - kLineHeight, kScrollStep));
        }
        break;
    default:
        break;
    }
}

void
SidePane::Scroll(int32_t dy)
{
    // Continue from where a running scroll animation ends, so that quick turns add up
    lv_point_t scroll_end;
    lv_obj_get_scroll_end(m_body, &scroll_end);

    const auto max_y = lv_obj_get_scroll_top(m_body) + lv_obj_get_scroll_bottom(m_body);
    const auto y = std::clamp<int32_t>(scroll_end.y + dy, 0, max_y);

    lv_obj_scroll_to_y(m_body, y, LV_ANIM_ON);
    UpdateOkButton();
}

bool
SidePane::AtBottom() const
{
    lv_point_t scroll_end;
    lv_obj_get_scroll_end(m_body, &scroll_end);

    const auto max_y = lv_obj_get_scroll_top(m_body) + lv_obj_get_scroll_bottom(m_body);

    return scroll_end.y >= max_y;
}

void
SidePane::UpdateOkButton()
{
    const auto color =
        AtBottom() ? lv_theme_get_color_primary(m_ok_button) : lv_palette_main(LV_PALETTE_GREY);

    lv_obj_set_style_bg_color(m_ok_button, color, LV_PART_MAIN);
}

void
SidePane::UpdateVisibility()
{
    lv_obj_set_flag(m_pane, LV_OBJ_FLAG_HIDDEN, !IsShown());
}
