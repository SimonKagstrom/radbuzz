#include "trip_meter_screen.hh"

#include <algorithm>
#include <cstddef>
#include <format>
#include <radbuzz_font_22.h>
#include <radbuzz_font_big.h>

/*
 * Distance traveled
 * Wh consumed
 * Wh regenerated
 * Average speed
 * Average consumption
 * Remaining range
 */

TripMeterScreen::TripMeterScreen(UserInterface& parent)
    : ScreenBase(parent)
    , m_stat_rows {
          {"Consumed", "Wh", StatValueKind::kConsumedWh},
          {"Regenerated", "Wh", StatValueKind::kRegeneratedWh},
          {"Trip average", "Wh/km", StatValueKind::kTripAverageWhPerKm},
      }
{
    static constexpr int kValueRightXOffset = 120;
    static constexpr int kUnitRightXOffset = 230;
    static constexpr int kValueColumnWidth = 170;
    static constexpr int kUnitColumnWidth = 130;
    static constexpr int kSideTextYOffset = 4;
    static constexpr int kFirstRowYOffset = -80;
    static constexpr int kRowSpacing = 80;

    m_screen = lv_obj_create(nullptr);

    std::size_t row_index = 0;
    for (auto& row : m_stat_rows)
    {
        const int y_offset = kFirstRowYOffset + static_cast<int>(row_index) * kRowSpacing;

        row.value = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.value, &radbuzz_font_big, LV_PART_MAIN);
        lv_obj_set_width(row.value, kValueColumnWidth);
        lv_obj_set_style_text_align(row.value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_label_set_text(row.value, "0");

        row.label = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.label, &radbuzz_font_22, LV_PART_MAIN);
        lv_label_set_text(row.label, row.label_text);
        const lv_coord_t label_width = lv_obj_get_width(row.label);
        lv_obj_align(row.label,
                     LV_ALIGN_CENTER,
                     -kUnitRightXOffset - (label_width / 2),
                     y_offset + kSideTextYOffset);

        row.unit = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.unit, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_width(row.unit, kUnitColumnWidth);
        lv_obj_set_style_text_align(row.unit, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_label_set_text(row.unit, row.unit_text);
        lv_obj_align(
            row.unit, LV_ALIGN_CENTER, kUnitRightXOffset - 20, y_offset + kSideTextYOffset);

        lv_obj_align(
            row.value, LV_ALIGN_CENTER, kValueRightXOffset - (kValueColumnWidth / 2), y_offset);

        ++row_index;
    }

    lv_style_init(&m_style_bar_bg);
    lv_style_set_border_color(&m_style_bar_bg, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&m_style_bar_bg, 2);
    lv_style_set_pad_all(&m_style_bar_bg, 6); /*To make the indicator smaller*/
    lv_style_set_radius(&m_style_bar_bg, 6);
    lv_style_set_anim_duration(&m_style_bar_bg, 1000);

    lv_style_init(&m_style_bar_indicator);
    lv_style_set_bg_opa(&m_style_bar_indicator, LV_OPA_COVER);
    lv_style_set_bg_color(&m_style_bar_indicator, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_radius(&m_style_bar_indicator, 3);


    //    m_consumption_bar = lv_bar_create(m_screen);
    //    lv_bar_set_range(m_consumption_bar, 0, 60);
    //
    //
    //    lv_obj_remove_style_all(m_consumption_bar); /*To have a clean start*/
    //    lv_obj_add_style(m_consumption_bar, &m_style_bar_bg, 0);
    //    lv_obj_add_style(m_consumption_bar, &m_style_bar_indicator, LV_PART_INDICATOR);
    //
    //    lv_obj_set_size(m_consumption_bar, 200, 20);
    //    lv_obj_align(m_consumption_bar, LV_ALIGN_CENTER, 0, 125);
}

void
TripMeterScreen::Update()
{
    static constexpr int kFirstRowYOffset = -80;
    static constexpr int kRowSpacing = 80;

    auto ro = m_parent.m_state.CheckoutReadonly();

    std::size_t row_index = 0;
    for (auto& row : m_stat_rows)
    {
        const int y_offset = kFirstRowYOffset + static_cast<int>(row_index) * kRowSpacing;
        uint32_t value = 0;
        switch (row.value_kind)
        {
        case StatValueKind::kConsumedWh:
            value = ro.Get<AS::wh_consumed>();
            break;
        case StatValueKind::kRegeneratedWh:
            value = ro.Get<AS::wh_regenerated>();
            break;
        case StatValueKind::kTripAverageWhPerKm: {
            const uint32_t distance_traveled = ro.Get<AS::distance_traveled>();
            const uint32_t wh_consumed = ro.Get<AS::wh_consumed>();
            const uint64_t average_consumption =
                distance_traveled > 0
                    ? (static_cast<uint64_t>(wh_consumed) * 1000ULL) / distance_traveled
                    : 0ULL;
            value = static_cast<uint32_t>(std::min<uint64_t>(average_consumption, 60ULL));
            break;
        }
        case StatValueKind::kValueCount:
            break;
        }

        lv_label_set_text(row.value, std::format("{}", value).c_str());

        static constexpr int kValueRightXOffset = 120;
        static constexpr int kValueColumnWidth = 170;
        lv_obj_align(
            row.value, LV_ALIGN_CENTER, kValueRightXOffset - (kValueColumnWidth / 2), y_offset);

        ++row_index;
    }

    //    const uint32_t distance_traveled = ro.Get<AS::distance_traveled>();
    //    const uint32_t wh_consumed = ro.Get<AS::wh_consumed>();
    //    const uint64_t average_consumption =
    //        distance_traveled > 0 ? (static_cast<uint64_t>(wh_consumed) * 1000ULL) / distance_traveled
    //                              : 0ULL;
    //    const auto average_consumption_i32 =
    //        static_cast<int32_t>(std::min<uint64_t>(average_consumption, 60ULL));
    //    lv_bar_set_value(m_consumption_bar, average_consumption_i32, LV_ANIM_ON);
}

void
TripMeterScreen::HandleInput(hal::IInput::EventType event)
{
    switch (event)
    {
    default:
        break;
    }
}
