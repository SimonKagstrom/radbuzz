#include "trip_meter_screen.hh"

#include "map_screen.hh"

#include <algorithm>
#include <cstddef>
#include <format>
#include <radbuzz_font_22.h>
#include <radbuzz_font_60.h>

constexpr int kValueRightXOffset = 120;
constexpr int kLabelColumnWidth = 220;
constexpr int kValueColumnWidth = 180;
constexpr int kUnitColumnWidth = 130;
constexpr int kLabelToValueGap = 24;
constexpr int kValueToUnitGap = 5;
constexpr int kFirstRowYOffset = 0;
constexpr int kRowSpacing = kPixelSize_radbuzz_font_60 + 10;

namespace
{
auto
GetSideTextBaselineYOffset()
{
    return radbuzz_font_22.base_line - radbuzz_font_60.base_line;
}
} // namespace

/*
 * Distance traveled
 * Average speed
 * Remaining range
 */

TripMeterScreen::TripMeterScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
    , m_stat_rows {
          {"SoC", "%", StatValueKind::kSoc},
          {"Controller/Motor", "°C", StatValueKind::kTemperature},
          {"Consumed", "Wh", StatValueKind::kConsumedWh},
          {"Regenerated", "Wh", StatValueKind::kRegeneratedWh},
          {"Trip average", "Wh/km", StatValueKind::kTripAverageWhPerKm},
          {"Trip max speed", "km/h", StatValueKind::kTripMaxSpeed},
          {"Trip distance", "m", StatValueKind::kTripDistance},
      }
{
    const auto side_text_baseline_y_offset = GetSideTextBaselineYOffset();

    std::size_t row_index = 0;
    for (auto& row : m_stat_rows)
    {
        const int y_offset = kFirstRowYOffset + static_cast<int>(row_index) * kRowSpacing;

        row.value = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.value, &radbuzz_font_60, LV_PART_MAIN);
        lv_obj_set_width(row.value, kValueColumnWidth);
        lv_obj_set_style_text_align(row.value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_label_set_text(row.value, "0");

        row.label = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.label, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_width(row.label, kLabelColumnWidth);
        lv_label_set_text(row.label, row.label_text);
        lv_obj_set_style_text_align(row.label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

        row.unit = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.unit, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_width(row.unit, kUnitColumnWidth);
        lv_obj_set_style_text_align(row.unit, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_label_set_text(row.unit, row.unit_text);

        lv_obj_align(
            row.value, LV_ALIGN_TOP_MID, kValueRightXOffset - (kValueColumnWidth / 2), y_offset);
        lv_obj_align_to(row.label,
                        row.value,
                        LV_ALIGN_OUT_LEFT_BOTTOM,
                        -kLabelToValueGap,
                        side_text_baseline_y_offset);
        lv_obj_align_to(row.unit,
                        row.value,
                        LV_ALIGN_OUT_RIGHT_BOTTOM,
                        kValueToUnitGap,
                        side_text_baseline_y_offset);

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
    auto ro = m_parent.m_state.CheckoutReadonly();
    const int side_text_baseline_y_offset = GetSideTextBaselineYOffset();

    auto trip_start = m_parent.m_current_trip_start;

    std::size_t row_index = 0;
    for (auto& row : m_stat_rows)
    {
        const int y_offset = kFirstRowYOffset + static_cast<int>(row_index) * kRowSpacing;
        std::string value_text {"0"};
        std::string unit_text {row.unit_text};
        std::string label_text {row.label_text};

        switch (row.value_kind)
        {
        case StatValueKind::kSoc:
            value_text = std::format("{}", ro.Get<AS::battery_soc>());
            break;
        case StatValueKind::kConsumedWh: {
            const auto consumed_wh = ro.Get<AS::wh_consumed>() - trip_start.start_wh_consumed;
            if (consumed_wh >= 1000)
            {
                unit_text = "kWh";
                value_text = std::format("{:.1f}", static_cast<float>(consumed_wh) / 1000.0f);
            }
            else
            {
                value_text = std::format("{:.1f}", consumed_wh);
            }
            break;
        }
        case StatValueKind::kRegeneratedWh: {
            const auto regenerated_wh =
                ro.Get<AS::wh_regenerated>() - trip_start.start_wh_regenerated;
            if (regenerated_wh >= 1000)
            {
                unit_text = "kWh";
                value_text = std::format("{:.1f}", static_cast<float>(regenerated_wh) / 1000.0f);
            }
            else
            {
                value_text = std::format("{:.1f}", regenerated_wh);
            }
            break;
        }
        case StatValueKind::kTripMaxSpeed:
            value_text = std::format("{}", ro.Get<AS::max_speed>());
            break;
        case StatValueKind::kTripDistance: {
            auto distance_m = ro.Get<AS::distance_traveled>() - trip_start.start_distance;

            if (distance_m >= 1000)
            {
                float distance_km = distance_m / 1000.0f;
                unit_text = "km";
                value_text = std::format("{:.1f}", distance_km);
            }
            else
            {
                unit_text = "m";
                value_text = std::format("{}", distance_m);
            }
        }
        break;
        case StatValueKind::kTripAverageWhPerKm: {
            // For the trip, not the odometer
            const uint32_t total_distance_m = ro.Get<AS::distance_traveled>();
            const uint32_t trip_distance_m = total_distance_m >= trip_start.start_distance
                                                 ? (total_distance_m - trip_start.start_distance)
                                                 : 0;

            const float total_wh_consumed = ro.Get<AS::wh_consumed>();
            const float trip_wh_consumed =
                std::max(0.0f, total_wh_consumed - trip_start.start_wh_consumed);

            const float average_consumption =
                trip_distance_m > 0 ? (trip_wh_consumed * 1000.0f) / trip_distance_m : 0.0f;
            value_text = std::format("{:.1f}", std::min(average_consumption, 60.0f));
            break;
        }

        case StatValueKind::kTemperature: {
            const auto controller_temp = ro.Get<AS::controller_temperature>();
            const auto motor_temp = ro.Get<AS::motor_temperature>();

            if (motor_temp != 0)
            {
                // Not mounted on all motors (like mine)
                value_text = std::format("{}/{}", controller_temp, motor_temp);
                label_text = "Controller/Motor";
            }
            else
            {
                // Should always be valid, since it comes from the VESC
                value_text = std::format("{}", controller_temp);
                label_text = "Controller";
            }
            break;
        }
        case StatValueKind::kValueCount:
            break;
        }

        lv_label_set_text(row.label, label_text.c_str());
        lv_label_set_text(row.value, value_text.c_str());
        lv_label_set_text(row.unit, unit_text.c_str());

        static constexpr int kValueRightXOffset = 120;
        static constexpr int kValueColumnWidth = 170;
        lv_obj_align(
            row.value, LV_ALIGN_TOP_MID, kValueRightXOffset - (kValueColumnWidth / 2), y_offset);
        lv_obj_align_to(row.label,
                        row.value,
                        LV_ALIGN_OUT_LEFT_BOTTOM,
                        -kLabelToValueGap,
                        side_text_baseline_y_offset);
        lv_obj_align_to(row.unit,
                        row.value,
                        LV_ALIGN_OUT_RIGHT_BOTTOM,
                        kValueToUnitGap,
                        side_text_baseline_y_offset);

        ++row_index;
    }
}

void
TripMeterScreen::HandleInput(const Input::Event& event)
{
    // Ugly
    auto map_screen = static_cast<MapScreen*>(m_parent.m_map_screen.get());

    switch (event.type)
    {
    case hal::IInput::EventType::kLeft:
        map_screen->SetZoom(kDefaultZoom);
        m_parent.ActivateScreen(*m_parent.m_map_screen);
        break;
    case hal::IInput::EventType::kRight:
        map_screen->SetZoom(kLandscapeZoom);
        m_parent.ActivateScreen(*m_parent.m_map_screen);
        break;
    case hal::IInput::EventType::kButtonDown:
        m_parent.ActivateScreen(*m_parent.m_settings_menu_screen);
        break;
    default:
        break;
    }
}
