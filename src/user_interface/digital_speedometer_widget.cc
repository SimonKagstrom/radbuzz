#include "digital_speedometer_widget.hh"

#include <format>
#include <radbuzz_font_16.h>
#include <radbuzz_font_60.h>

DigitalSpeedometerWidget::DigitalSpeedometerWidget(lv_obj_t* parent)
{
    // Push left rounded corners off-screen for the navigation pane while keeping right corners.
    constexpr int kLeftCornerClipPx = 16;
    constexpr int kPaneCornerRadius = 18;

    // Digital speedometer
    m_speedometer_box = lv_obj_create(parent);
    lv_obj_set_size(m_speedometer_box, 128 + kLeftCornerClipPx, 128);
    lv_obj_align(m_speedometer_box, LV_ALIGN_TOP_LEFT, -32, -32);
    lv_obj_set_style_border_width(m_speedometer_box, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(m_speedometer_box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_speedometer_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_speedometer_box, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_speedometer_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(m_speedometer_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(m_speedometer_box, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_opa(m_speedometer_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(m_speedometer_box, kPaneCornerRadius, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(m_speedometer_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_speedometer_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(m_speedometer_box, LV_OBJ_FLAG_CLICKABLE);

    m_speed_digits_label = lv_label_create(m_speedometer_box);
    lv_obj_set_style_text_font(m_speed_digits_label, &radbuzz_font_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speed_digits_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_speed_digits_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_align(m_speed_digits_label, LV_ALIGN_CENTER);
    lv_obj_set_pos(m_speed_digits_label, 8, 4);
    lv_label_set_text(m_speed_digits_label, "0");

    m_gps_speed_label = lv_label_create(m_speedometer_box);
    lv_obj_set_style_text_font(m_gps_speed_label, &radbuzz_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_gps_speed_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_gps_speed_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_align(m_gps_speed_label, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(m_gps_speed_label, 8, 4);
    lv_label_set_text(m_gps_speed_label, "0");
}

void
DigitalSpeedometerWidget::Update(ApplicationState& state)
{
    auto ro = state.CheckoutReadonly();
    auto conf = ro.Get<AS::configuration>();
    auto gps_speed = std::max(0.0f, ro.Get<AS::position>()->speed);

    lv_label_set_text(m_speed_digits_label, std::format("{}", ro.Get<AS::speed>()).c_str());
    lv_label_set_text(m_gps_speed_label,
                      std::format("{}", static_cast<uint8_t>(gps_speed)).c_str());

    if (conf->speedometer_type == SpeedometerType::kDigital ||
        conf->speedometer_type == SpeedometerType::kBoth)
    {
        lv_obj_remove_flag(m_speedometer_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m_speed_digits_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flag(m_gps_speed_label, LV_OBJ_FLAG_HIDDEN, !conf->show_gps_speed);
    }
    else
    {
        lv_obj_add_flag(m_speedometer_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_speed_digits_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_gps_speed_label, LV_OBJ_FLAG_HIDDEN);
    }
}
