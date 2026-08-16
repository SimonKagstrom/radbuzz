#include "digital_speedometer_widget.hh"

#include "time_string.hh"
#include "user_interface.hh"

#include <format>
#include <radbuzz_font_16.h>
#include <radbuzz_font_22.h>
#include <radbuzz_font_60.h>

constexpr auto kSpeedometerBox = 0;
constexpr auto kDistanceBox = 1;

constexpr auto kDistanceIndex = 0;
constexpr auto kTripTimeIndex = 1;
constexpr int kDistanceValueXOffset = 8;
constexpr int kDistanceFirstRowYOffset = 26;
constexpr int kDistanceRowSpacing = 34;
constexpr int kDistanceValueToUnitGap = 4;

DigitalSpeedometerWidget::DigitalSpeedometerWidget(lv_obj_t* parent)
{
    // Push left rounded corners off-screen for the navigation pane while keeping right corners.
    constexpr int kCornerClipPx = 16;
    constexpr int kPaneCornerRadius = 18;

    // Digital speedometer
    for (auto& box : m_boxes)
    {
        box = lv_obj_create(parent);

        lv_obj_set_size(box, 128 + kCornerClipPx, kBoxDimensions + 32);
        lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(box, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(box, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(box, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_bg_color(box, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_image_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_grad_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(box, kPaneCornerRadius, LV_PART_MAIN);
        lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_align(m_boxes[kSpeedometerBox], LV_ALIGN_TOP_LEFT, -32, -32);
    lv_obj_align(m_boxes[kDistanceBox], LV_ALIGN_TOP_RIGHT, 32, -32);

    m_speed_digits_label = lv_label_create(m_boxes[kSpeedometerBox]);
    lv_obj_set_style_text_font(m_speed_digits_label, &radbuzz_font_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speed_digits_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_speed_digits_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_align(m_speed_digits_label, LV_ALIGN_CENTER);
    lv_obj_set_pos(m_speed_digits_label, 12, 14);
    lv_label_set_text(m_speed_digits_label, "0");

    m_gps_speed_label = lv_label_create(m_boxes[kSpeedometerBox]);
    lv_obj_set_style_text_font(m_gps_speed_label, &radbuzz_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_gps_speed_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_gps_speed_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_align(m_gps_speed_label, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(m_gps_speed_label, 8, 4);
    lv_label_set_text(m_gps_speed_label, "0");

    for (auto& label : m_distance_labels)
    {
        label = lv_label_create(m_boxes[kDistanceBox]);
        lv_obj_set_style_text_font(label, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    }

    for (auto& label : m_distance_unit_labels)
    {
        label = lv_label_create(m_boxes[kDistanceBox]);
        lv_obj_set_style_text_font(label, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    }

    for (std::size_t i = 0; i < m_distance_labels.size(); ++i)
    {
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
DigitalSpeedometerWidget::Update(ApplicationState& state, bool show_speedometer, bool show_distance)
{
    auto ro = state.CheckoutReadonly();
    auto conf = ro.Get<AS::configuration>();
    auto gps_speed = std::max(0.0f, ro.Get<AS::position>()->speed);

    if (show_speedometer && (conf->speedometer_type == SpeedometerType::kDigital ||
                             conf->speedometer_type == SpeedometerType::kBoth))
    {
        lv_label_set_text(m_speed_digits_label, std::format("{}", ro.Get<AS::speed>()).c_str());
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
        auto distance = ro.Get<AS::trip_distance>();

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


        auto seconds = ro.Get<AS::trip_duration>().count();
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
