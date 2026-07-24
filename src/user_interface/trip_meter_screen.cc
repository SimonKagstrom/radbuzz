#include "trip_meter_screen.hh"

#include "map_screen.hh"

#include <algorithm>
#include <cstddef>
#include <format>
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
    lv_obj_set_scrollbar_mode(m_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);

    m_stat_rows.reserve(7);
    m_stat_rows.emplace_back(StatRow {"SoC", "%", StatValueKind::kSoc});
    m_stat_rows.emplace_back(StatRow {"Controller/Motor", "°C", StatValueKind::kTemperature});
    m_stat_rows.emplace_back(StatRow {"Consumed/regenerated",
                                      "Wh",
                                      StatValueKind::kConsumedWh,
                                      std::make_unique<SecondColumnStatRow>("Wh")});
    m_stat_rows.emplace_back(
        StatRow {"Average consumption", "Wh/km", StatValueKind::kTripAverageWhPerKm});
    m_stat_rows.emplace_back(StatRow {"Distance/Odometer",
                                      "m",
                                      StatValueKind::kTripDistance,
                                      std::make_unique<SecondColumnStatRow>("m")});
    m_stat_rows.emplace_back(StatRow {"Time", "s", StatValueKind::kTime});
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
}

void
TripMeterScreen::Update()
{
    auto ro = m_parent.m_state.CheckoutReadonly();
    const int side_text_baseline_y_offset = GetSideTextBaselineYOffset();

    auto trip_start = m_parent.m_current_trip_start;

    auto since_trip_start = os::GetTimeStamp() - ro.Get<AS::trip_start_time>();
    if (since_trip_start == 0ms)
    {
        // No divide by zero, so assume at least 1 millisecond has passed
        since_trip_start = 1ms;
    }

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
            value_text = std::format("{}", ro.Get<AS::max_speed>());

            debug_assert(row.second_column != nullptr);
            const uint32_t total_distance_m = ro.Get<AS::odometer>();
            const uint32_t trip_distance_m = total_distance_m >= trip_start.start_distance
                                                 ? (total_distance_m - trip_start.start_distance)
                                                 : 0;

            const float elapsed_seconds = static_cast<float>(since_trip_start.count()) / 1000.0f;
            const float average_speed_kmh =
                elapsed_seconds > 0.0f
                    ? (static_cast<float>(trip_distance_m) * 3.6f) / elapsed_seconds
                    : 0.0f;

            lv_label_set_text(row.second_column->value,
                              std::format("{:.0f}", average_speed_kmh).c_str());
            break;
        }
        case StatValueKind::kTripDistance: {
            auto odometer_m = ro.Get<AS::odometer>();
            auto distance_m = odometer_m - trip_start.start_distance;

            debug_assert(row.second_column != nullptr);
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

            if (odometer_m >= 1000)
            {
                float distance_km = odometer_m / 1000.0f;

                lv_label_set_text(row.second_column->value,
                                  std::format("{:.1f}", distance_km).c_str());
                lv_label_set_text(row.second_column->unit, "km");
            }
            else
            {
                lv_label_set_text(row.second_column->value, std::format("{}", odometer_m).c_str());
                lv_label_set_text(row.second_column->unit, "m");
            }
            break;
        }

        case StatValueKind::kTripAverageWhPerKm: {
            // For the trip, not the odometer
            const uint32_t total_distance_m = ro.Get<AS::odometer>();
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
        case StatValueKind::kTime: {
            auto seconds = ro.Get<AS::trip_duration>().count();

            if (seconds > 60)
            {
                if (seconds > 3600)
                {
                    // Hours
                    value_text = std::format(
                        "{:02}:{:02}:{:02}", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
                }
                else
                {
                    // Minutes
                    value_text = std::format("{:02}:{:02}", seconds / 60, seconds % 60);
                }
                unit_text = "";
            }
            else
            {
                value_text = std::format("{}", seconds);
                unit_text = "s";
            }
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
