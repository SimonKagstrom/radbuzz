#include "trip_meter_screen.hh"

#include "map_screen.hh"
#include "time_string.hh"

#include <algorithm>
#include <cstddef>
#include <format>
#include <radbuzz_font_16.h>
#include <radbuzz_font_22.h>
#include <radbuzz_font_60.h>

constexpr int kValueRightXOffset = 80;
constexpr int kLabelColumnWidth = 300;
constexpr int kValueColumnWidth = 180;
constexpr int kUnitColumnWidth = 100;
constexpr int kLabelToValueGap = 24;
constexpr int kValueToUnitGap = 5;
constexpr int kFirstRowYOffset = 0;
constexpr int kRowSpacing = kPixelSize_radbuzz_font_60 + 10;
constexpr int kSecondColumnRightXOffset = 310;

namespace
{
auto
GetSideTextBaselineYOffset()
{
    return radbuzz_font_22.base_line - radbuzz_font_60.base_line;
}
} // namespace


TripMeterScreen::TripMeterScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
{
    const lv_color_t kTripMeterBackgroundColor = lv_color_make(47, 47, 58);

    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_screen, kTripMeterBackgroundColor, 0);

    lv_obj_set_scrollbar_mode(m_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);

    m_stat_rows.reserve(7);
    m_stat_rows.emplace_back(StatRow {
        "SoC/range", "%", StatValueKind::kSoc, std::make_unique<SecondColumnStatRow>("km")});
    m_stat_rows.emplace_back(StatRow {"Controller/Motor", "°C", StatValueKind::kTemperature});
    m_stat_rows.emplace_back(StatRow {"Trip time", "s", StatValueKind::kTime});
    m_stat_rows.emplace_back(StatRow {"Distance", "m", StatValueKind::kTripDistance});
    m_stat_rows.emplace_back(
        StatRow {"Average consumption", "Wh/km", StatValueKind::kTripAverageWhPerKm});
    m_stat_rows.emplace_back(StatRow {"Consumed/regenerated",
                                      "Wh",
                                      StatValueKind::kConsumedWh,
                                      std::make_unique<SecondColumnStatRow>("Wh")});
    m_stat_rows.emplace_back(StatRow {"Max/average speed",
                                      "km/h",
                                      StatValueKind::kTripMaxSpeed,
                                      std::make_unique<SecondColumnStatRow>("km/h")});

    const auto side_text_baseline_y_offset = GetSideTextBaselineYOffset();

    std::size_t row_index = 0;
    int y_offset = 0;
    for (auto& row : m_stat_rows)
    {
        y_offset = kFirstRowYOffset + static_cast<int>(row_index) * kRowSpacing;

        row.label = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.label, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_width(row.label, kLabelColumnWidth);
        lv_label_set_text(row.label, row.label_text);
        lv_obj_set_style_text_align(row.label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

        row.value = lv_label_create(m_screen);
        lv_obj_set_style_text_font(row.value, &radbuzz_font_60, LV_PART_MAIN);
        lv_obj_set_width(row.value, kValueColumnWidth);
        lv_obj_set_style_text_align(row.value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_label_set_text(row.value, "0");

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

        if (row.second_column)
        {
            row.second_column->value = lv_label_create(m_screen);
            lv_obj_set_style_text_font(row.second_column->value, &radbuzz_font_60, LV_PART_MAIN);
            lv_obj_set_width(row.second_column->value, kValueColumnWidth);
            lv_obj_set_style_text_align(
                row.second_column->value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
            lv_label_set_text(row.second_column->value, "0");

            row.second_column->unit = lv_label_create(m_screen);
            lv_obj_set_style_text_font(row.second_column->unit, &radbuzz_font_22, LV_PART_MAIN);
            lv_obj_set_width(row.second_column->unit, kUnitColumnWidth);
            lv_obj_set_style_text_align(row.second_column->unit, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
            lv_label_set_text(row.second_column->unit, row.second_column->unit_text);

            lv_obj_align(row.second_column->value,
                         LV_ALIGN_TOP_MID,
                         kSecondColumnRightXOffset - (kValueColumnWidth / 2),
                         y_offset);
            lv_obj_align_to(row.second_column->unit,
                            row.second_column->value,
                            LV_ALIGN_OUT_RIGHT_BOTTOM,
                            kValueToUnitGap,
                            side_text_baseline_y_offset);
        }


        ++row_index;
    }

    m_max_power_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_max_power_label, &radbuzz_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_max_power_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_max_power_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(m_max_power_label, LV_ALIGN_TOP_RIGHT, -10, 1);
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
            lv_label_set_text(row.second_column->value,
                              std::format("{}", ro.Get<AS::estimated_range_km>()).c_str());
            break;
        case StatValueKind::kConsumedWh: {

            const auto total_wh_consumed = ro.Get<AS::wh_consumed>();
            const auto consumed_wh = total_wh_consumed - trip_start.start_wh_consumed;
            if (consumed_wh >= 1000)
            {
                unit_text = "kWh";
                value_text = std::format("{:.1f}", static_cast<float>(consumed_wh) / 1000.0f);
            }
            else
            {
                value_text = std::format("{:.1f}", consumed_wh);
            }


            debug_assert(row.second_column != nullptr);
            const auto regenerated_wh =
                ro.Get<AS::wh_regenerated>() - trip_start.start_wh_regenerated;

            if (regenerated_wh >= 1000)
            {
                lv_label_set_text(
                    row.second_column->value,
                    std::format("{:.1f}", static_cast<float>(regenerated_wh) / 1000.0f).c_str());
                lv_label_set_text(row.second_column->unit, "kWh");
            }
            else
            {
                lv_label_set_text(
                    row.second_column->value,
                    std::format("{:.1f}", static_cast<float>(regenerated_wh)).c_str());
                lv_label_set_text(row.second_column->unit, "Wh");
            }
            break;
        }
        case StatValueKind::kTripMaxSpeed: {
            value_text = std::format("{}", ro.Get<AS::trip_max_speed>());

            debug_assert(row.second_column != nullptr);
            lv_label_set_text(row.second_column->value,
                              std::format("{}", ro.Get<AS::trip_average_speed>()).c_str());
            break;
        }
        case StatValueKind::kTripDistance: {
            auto odometer_m = ro.Get<AS::odometer>();
            auto distance_m = ro.Get<AS::trip_distance>();

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
            break;
        }

        case StatValueKind::kTripAverageWhPerKm: {
            // For the trip, not the odometer
            const uint32_t total_distance_m = ro.Get<AS::odometer>();
            const uint32_t trip_distance_m = ro.Get<AS::trip_distance>();

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
        case StatValueKind::kTime: {
            auto seconds = ro.Get<AS::trip_duration>().count();

            value_text = std::format("{}", seconds);
            unit_text = seconds > 60 ? "" : "s";
        }
        break;
        case StatValueKind::kValueCount:
            break;
        }

        lv_label_set_text(row.label, label_text.c_str());
        lv_label_set_text(row.value, value_text.c_str());
        lv_label_set_text(row.unit, unit_text.c_str());

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

    auto max_power = ro.Get<AS::configuration>()->max_watts;
    lv_label_set_text(m_max_power_label, std::format("+{} W", max_power).c_str());
}

void
TripMeterScreen::HandleInput(const Input::Event& event)
{
    int dx = 0;
    auto map_screen = static_cast<MapScreen*>(m_parent.m_map_screen.get());

    switch (event.type)
    {
    case hal::IInput::EventType::kLeft:
        dx = -1;
        break;
    case hal::IInput::EventType::kRight:
        dx = 1;
        break;
    case hal::IInput::EventType::kButtonDown:
        m_parent.ActivateScreen(*m_parent.m_settings_menu_screen);
        return;
    default:
        break;
    }


    debug_assert(m_parent.m_lvgl_touch_input_dev);

    if (m_parent.m_touch_state == LV_INDEV_STATE_PRESSED)
    {
        lv_point_t touch_vector {0, 0};

        lv_indev_get_vect(m_parent.m_lvgl_touch_input_dev, &touch_vector);

        // Swipes anywhere
        const auto gesture_dir = lv_indev_get_gesture_dir(m_parent.m_lvgl_touch_input_dev);
        dx = 1 * (gesture_dir == LV_DIR_LEFT) - 1 * (gesture_dir == LV_DIR_RIGHT);
    }

    if (dx == -1)
    {
        map_screen->SetZoom(kDefaultZoom);
        m_parent.ActivateScreen(*m_parent.m_map_screen);
    }
    else if (dx == 1)
    {
        map_screen->SetZoom(kLandscapeZoom);
        m_parent.ActivateScreen(*m_parent.m_map_screen);
    }
}
