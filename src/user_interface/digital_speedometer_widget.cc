#include "digital_speedometer_widget.hh"

#include "time_string.hh"
#include "ui_style.hh"
#include "user_interface.hh"

#include <format>
#include <radbuzz_font_16.h>
#include <radbuzz_font_60.h>

constexpr auto kSpeedometerBox = 0;
constexpr auto kDistanceBox = 1;

constexpr auto kDistanceIndex = 0;
constexpr auto kTripTimeIndex = 1;
constexpr int kDistanceValueXOffset = 8;
constexpr int kDistanceFirstRowYOffset = 26;
constexpr int kDistanceRowSpacing = 34;
constexpr int kDistanceValueToUnitGap = 4;

// Pushed outside the screen, so that only the lower right corner is rounded
constexpr int kSpeedometerBoxOffset = -32;

DigitalSpeedometerWidget::DigitalSpeedometerWidget(lv_obj_t* parent)
{
    // Push left rounded corners off-screen for the navigation pane while keeping right corners.
    constexpr int kCornerClipPx = 16;
    constexpr int kPaneCornerRadius = 18;

    // Digital speedometer
    for (auto& box : m_boxes)
    {
        box = ui::CreatePane(parent, kPaneCornerRadius);
        lv_obj_set_size(box, 128 + kCornerClipPx, kBoxDimensions + 32);
    }
    lv_obj_align(
        m_boxes[kSpeedometerBox], LV_ALIGN_TOP_LEFT, kSpeedometerBoxOffset, kSpeedometerBoxOffset);
    lv_obj_align(m_boxes[kDistanceBox], LV_ALIGN_TOP_RIGHT, 32, -32);

    m_speed_digits_label = ui::CreateLabel(m_boxes[kSpeedometerBox], &radbuzz_font_60, "0");
    lv_obj_set_align(m_speed_digits_label, LV_ALIGN_CENTER);
    lv_obj_set_pos(m_speed_digits_label, 12, 14);

    m_gps_speed_label = ui::CreateLabel(m_boxes[kSpeedometerBox], &radbuzz_font_16, "0");
    lv_obj_set_align(m_gps_speed_label, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(m_gps_speed_label, 8, 4);

    for (std::size_t i = 0; i < m_distance_labels.size(); ++i)
    {
        // The units are aligned once, after the width of the placeholder (room for the values)
        m_distance_labels[i] =
            ui::CreateLabel(m_boxes[kDistanceBox], nullptr, ui::kLvglPlaceholderText);
        m_distance_unit_labels[i] = ui::CreateLabel(m_boxes[kDistanceBox]);

        lv_obj_align(m_distance_labels[i],
                     LV_ALIGN_TOP_LEFT,
                     kDistanceValueXOffset,
                     kDistanceFirstRowYOffset + static_cast<int>(i) * kDistanceRowSpacing);
        lv_obj_align_to(m_distance_unit_labels[i],
                        m_distance_labels[i],
                        LV_ALIGN_OUT_RIGHT_MID,
                        kDistanceValueToUnitGap,
                        0);
    }

    lv_label_set_text(m_distance_labels[kDistanceIndex], "0");
    lv_label_set_text(m_distance_unit_labels[kDistanceIndex], "m");
    lv_label_set_text(m_distance_labels[kTripTimeIndex], "0");
    lv_label_set_text(m_distance_unit_labels[kTripTimeIndex], "");
}

void
DigitalSpeedometerWidget::Update(ApplicationState& state,
                                 bool show_speedometer,
                                 bool show_distance,
                                 int32_t left_x)
{
    if (left_x != m_left_x)
    {
        // Next to the side pane, which is drawn on top and hides the left corners
        m_left_x = left_x;
        lv_obj_align(m_boxes[kSpeedometerBox],
                     LV_ALIGN_TOP_LEFT,
                     m_left_x + kSpeedometerBoxOffset,
                     kSpeedometerBoxOffset);
    }

    auto conf = state.Get<AS::configuration>();
    auto gps_speed = std::max(0.0f, state.Get<AS::position>()->speed);

    if (show_speedometer && (conf->speedometer_type == SpeedometerType::kDigital ||
                             conf->speedometer_type == SpeedometerType::kBoth))
    {
        lv_label_set_text(m_speed_digits_label, std::format("{}", state.Get<AS::speed>()).c_str());
        lv_label_set_text(m_gps_speed_label,
                          std::format("{}", static_cast<uint8_t>(gps_speed)).c_str());

        lv_obj_remove_flag(m_boxes[kSpeedometerBox], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m_speed_digits_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flag(m_gps_speed_label, LV_OBJ_FLAG_HIDDEN, !conf->show_gps_speed);
    }
    else
    {
        lv_obj_add_flag(m_boxes[kSpeedometerBox], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_speed_digits_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_gps_speed_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (show_distance)
    {
        auto distance = state.Get<AS::trip_distance>();

        if (distance >= 1000)
        {
            float distance_km = distance / 1000.0f;
            lv_label_set_text(m_distance_labels[kDistanceIndex],
                              std::format("{:.1f}", distance_km).c_str());
            lv_label_set_text(m_distance_unit_labels[kDistanceIndex], "km");
        }
        else
        {
            lv_label_set_text(m_distance_labels[kDistanceIndex],
                              std::format("{}", distance).c_str());
            lv_label_set_text(m_distance_unit_labels[kDistanceIndex], "m");
        }


        auto seconds = state.Get<AS::trip_duration>().count();
        lv_label_set_text(m_distance_labels[kTripTimeIndex], SecondsToString(seconds).c_str());

        if (seconds >= 60)
        {
            lv_obj_add_flag(m_distance_unit_labels[kTripTimeIndex], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_remove_flag(m_distance_unit_labels[kTripTimeIndex], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_remove_flag(m_boxes[kDistanceBox], LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(m_boxes[kDistanceBox], LV_OBJ_FLAG_HIDDEN);
    }
}
