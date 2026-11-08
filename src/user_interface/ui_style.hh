#pragma once

#include <lvgl.h>

// Common LVGL object setups. Only what differs from the default (dark) theme is set, and shared
// styles are used instead of per-object (local) ones
namespace ui
{

// The text of new LVGL labels. Some layouts are aligned to labels before their real text is set,
// i.e., to the width of this
constexpr auto kLvglPlaceholderText = "Text";

// A black, borderless box. Display-only: not scrollable or clickable, so touches pass through
lv_obj_t* CreatePane(lv_obj_t* parent, int32_t radius = 0);

// An invisible box for grouping and layout: no background, border, radius or padding
lv_obj_t* CreateContainer(lv_obj_t* parent);

// A white label. The font is inherited (the theme font, with the symbol fallback) unless given
lv_obj_t* CreateLabel(lv_obj_t* parent, const lv_font_t* font = nullptr, const char* text = "");

// The background of the non-map screens. Not scrollable, since swipes switch screens
void StyleScreen(lv_obj_t* screen);

} // namespace ui
