#include "trip_meter_screen.hh"

#include <radbuzz_font_22.h>
#include <radbuzz_font_big.h>

/*
 * Distance travelled
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
    lv_obj_align(m_battery_millivolts_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(m_battery_millivolts_label, &radbuzz_font_big, LV_PART_MAIN);
    lv_label_set_long_mode(m_battery_millivolts_label, LV_LABEL_LONG_WRAP);
}

void
TripMeterScreen::Update()
{
    auto ro = m_parent.m_state.CheckoutReadonly();
    auto battery_mv = ro.Get<AS::battery_millivolts>();

    lv_label_set_text(m_battery_millivolts_label,
                      std::format("{} {}",
                                  ro.Get<AS::distance_travelled>(),
                                  static_cast<float>(ro.Get<AS::wh_consumed>()))
                          .c_str());
}
