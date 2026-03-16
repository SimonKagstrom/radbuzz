#include "trip_meter_screen.hh"

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
{
    m_screen = lv_obj_create(nullptr);

    // Temporary hack
    m_battery_millivolts_label = lv_label_create(m_screen);
    lv_obj_align(m_battery_millivolts_label, LV_ALIGN_CENTER, 0, -100);
    lv_obj_set_style_text_font(m_battery_millivolts_label, &radbuzz_font_big, LV_PART_MAIN);
    lv_label_set_long_mode(m_battery_millivolts_label, LV_LABEL_LONG_WRAP);

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


    m_consumption_bar = lv_bar_create(m_screen);
    lv_bar_set_range(m_consumption_bar, 0, 60);


    lv_obj_remove_style_all(m_consumption_bar); /*To have a clean start*/
    lv_obj_add_style(m_consumption_bar, &m_style_bar_bg, 0);
    lv_obj_add_style(m_consumption_bar, &m_style_bar_indicator, LV_PART_INDICATOR);

    lv_obj_set_size(m_consumption_bar, 200, 20);
    lv_obj_center(m_consumption_bar);
}

void
TripMeterScreen::Update()
{
    auto ro = m_parent.m_state.CheckoutReadonly();
    auto battery_mv = ro.Get<AS::battery_millivolts>();

    lv_label_set_text(
        m_battery_millivolts_label,
        std::format("{} {}", ro.Get<AS::speed>(), static_cast<float>(ro.Get<AS::wh_consumed>()))
            .c_str());

    auto average_consumption =
        static_cast<float>(ro.Get<AS::wh_consumed>()) / ro.Get<AS::distance_traveled>() * 1000.0f;
    lv_bar_set_value(m_consumption_bar, average_consumption, LV_ANIM_ON);
}

void
TripMeterScreen::HandleInput(hal::IInput::EventType event)
{
}
