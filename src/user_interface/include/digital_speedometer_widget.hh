#pragma once
#include "application_state.hh"

#include <lvgl.h>

class DigitalSpeedometerWidget
{
public:
    DigitalSpeedometerWidget(lv_obj_t* parent);

    void Update(ApplicationState& state);

private:
    lv_obj_t* m_speedometer_box {nullptr};
    lv_obj_t* m_speed_digits_label {nullptr};
    lv_obj_t* m_gps_speed_label {nullptr};
};