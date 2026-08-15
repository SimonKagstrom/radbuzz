#include "speedometer_only_screen.hh"

#include "map_screen.hh"
#include "radbuzz_font_120.h"
#include "radbuzz_font_22.h"
#include "radbuzz_font_40.h"
#include "time_string.hh"

SpeedometerOnlyScreen::SpeedometerOnlyScreen(UserInterface& parent)
    : UserInterface::ScreenBase(parent, lv_obj_create(nullptr))
{
    const lv_color_t kBackgroundColor = lv_color_make(47, 47, 58);

    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_screen, kBackgroundColor, 0);

    // Big speedometer in the center of the screen
    m_speedometer_label = lv_label_create(m_screen);
    lv_obj_align(m_speedometer_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(m_speedometer_label, &radbuzz_font_120, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speedometer_label, lv_color_white(), LV_PART_MAIN);

    m_speedometer_unit_label = lv_label_create(m_screen);
    lv_obj_align_to(m_speedometer_unit_label, m_speedometer_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
    lv_obj_set_style_text_font(m_speedometer_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speedometer_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_speedometer_unit_label, "km/h");


    m_battery_label = lv_label_create(m_screen);
    lv_obj_align(m_battery_label, LV_ALIGN_TOP_LEFT, 0, 4);
    lv_obj_set_style_text_font(m_battery_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_battery_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_battery_label, "Battery");

    m_battery_value_label = lv_label_create(m_screen);
    lv_obj_align_to(m_battery_value_label, m_battery_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_text_font(m_battery_value_label, &radbuzz_font_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_battery_value_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_battery_value_label, "100");

    m_battery_value_unit_label = lv_label_create(m_screen);
    lv_obj_align_to(
        m_battery_value_unit_label, m_battery_value_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, -4);
    lv_obj_set_style_text_font(m_battery_value_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_battery_value_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_battery_value_unit_label, "%");

    m_temperature_label = lv_label_create(m_screen);
    lv_obj_align(m_temperature_label, LV_ALIGN_TOP_RIGHT, -20, 4);
    lv_obj_set_style_text_font(m_temperature_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_temperature_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_temperature_label, "Controller/Motor/BMS/Cell");

    m_temperature_value_unit_label = lv_label_create(m_screen);
    lv_obj_align_to(m_temperature_value_unit_label,
                    m_temperature_label,
                    LV_ALIGN_OUT_RIGHT_BOTTOM,
                    -20,
                    kPixelSize_radbuzz_font_40 + 4);
    lv_obj_set_style_text_font(m_temperature_value_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_temperature_value_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_temperature_value_unit_label, "°C");

    m_temperature_value_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_temperature_value_label, &radbuzz_font_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_temperature_value_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(m_temperature_value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);


    m_trip_distance_value_label = lv_label_create(m_screen);
    lv_obj_align(m_trip_distance_value_label, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_set_style_text_font(m_trip_distance_value_label, &radbuzz_font_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_trip_distance_value_label, lv_color_white(), LV_PART_MAIN);

    m_trip_distance_label = lv_label_create(m_screen);
    lv_obj_align_to(
        m_trip_distance_label, m_trip_distance_value_label, LV_ALIGN_OUT_TOP_LEFT, 0, -4);
    lv_obj_set_style_text_font(m_trip_distance_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_trip_distance_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(m_trip_distance_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text(m_trip_distance_label, "Trip");


    m_power_label = lv_label_create(m_screen);
    lv_obj_align(m_power_label, LV_ALIGN_BOTTOM_MID, -16, -10);
    lv_obj_set_style_text_font(m_power_label, &radbuzz_font_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(m_power_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_power_label, lv_color_white(), LV_PART_MAIN);


    m_range_value = lv_label_create(m_screen);
    lv_obj_align(m_range_value, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_set_style_text_font(m_range_value, &radbuzz_font_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(m_range_value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_range_value, lv_color_white(), LV_PART_MAIN);

    m_range_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_range_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_align(m_range_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_range_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_range_label, "Range");
}

void
SpeedometerOnlyScreen::Update()
{
    lv_label_set_text(m_speedometer_label,
                      std::format("{}", m_parent.m_state.Get<AS::speed>()).c_str());

    lv_label_set_text(m_battery_value_label,
                      std::format("{}", m_parent.m_state.Get<AS::battery_soc>()).c_str());

    std::string temperature_text = "Controller";
    std::string temperature_value_text =
        std::format("{}", m_parent.m_state.Get<AS::controller_temperature>());

    if (m_parent.m_state.Get<AS::motor_temperature>() != 0)
    {
        temperature_text +=
            "/Motor" + std::to_string(m_parent.m_state.Get<AS::motor_temperature>());
        temperature_value_text +=
            "/" + std::to_string(m_parent.m_state.Get<AS::motor_temperature>());
    }
    if (auto bms = m_parent.m_state.Get<AS::bms_data>(); bms->valid)
    {
        temperature_text += "/BMS/Cell";
        temperature_value_text += "/" + std::to_string(bms->bms_temperature) + "/" +
                                  std::to_string(bms->highest_cell_temp);
    }

    lv_label_set_text(m_temperature_label, temperature_text.c_str());
    lv_label_set_text(m_temperature_value_label, temperature_value_text.c_str());

    lv_label_set_text(m_power_label,
                      std::format("{} W", m_parent.m_state.Get<AS::current_power_w>()).c_str());

    lv_label_set_text(m_range_value,
                      std::format("{} km", m_parent.m_state.Get<AS::estimated_range_km>()).c_str());

    std::string trip_value_text;
    auto trip_distance_m = m_parent.m_state.Get<AS::trip_distance>();

    if (trip_distance_m < 1000)
    {
        trip_value_text = std::format("{} m", trip_distance_m);
    }
    else
    {
        trip_value_text = std::format("{:.1f} km", trip_distance_m / 1000.0f);
    }
    trip_value_text += ", " + SecondsToString(m_parent.m_state.Get<AS::trip_duration>());

    lv_label_set_text(m_trip_distance_value_label, trip_value_text.c_str());


    // Dynamic alignment
    lv_obj_align_to(
        m_temperature_value_label, m_temperature_value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 2);
    lv_obj_align_to(
        m_trip_distance_label, m_trip_distance_value_label, LV_ALIGN_OUT_TOP_LEFT, 0, -4);
    lv_obj_align_to(m_range_label, m_range_value, LV_ALIGN_OUT_TOP_RIGHT, 0, -4);
}

void
SpeedometerOnlyScreen::HandleInput(const Input::Event& event)
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
        m_parent.ActivateScreen(*m_parent.m_trip_meter_screen);
    }
}

void
SpeedometerOnlyScreen::SetHelp(bool on)
{
}
