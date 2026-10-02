#pragma once
#include "application_state.hh"

#include <lvgl.h>

class DigitalSpeedometerWidget
{
public:
    friend class UserInterface;

    static constexpr auto kBoxDimensions = 96;

    DigitalSpeedometerWidget(lv_obj_t* parent);

    // left_x is where the speedometer starts, e.g., to the right of the side pane
    void Update(ApplicationState& state, bool show_speedometer, bool show_distance, int32_t left_x);

private:
    std::array<lv_obj_t*, 2> m_boxes {nullptr, nullptr};
    lv_obj_t* m_speed_digits_label {nullptr};
    lv_obj_t* m_gps_speed_label {nullptr};

    std::array<lv_obj_t*, 2> m_distance_labels {nullptr, nullptr};
    std::array<lv_obj_t*, 2> m_distance_unit_labels {nullptr, nullptr};

    int32_t m_left_x {0};
};