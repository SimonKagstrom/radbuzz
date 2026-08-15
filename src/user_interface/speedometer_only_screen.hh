#pragma once

#include "user_interface.hh"

class SpeedometerOnlyScreen : public UserInterface::ScreenBase
{
public:
    explicit SpeedometerOnlyScreen(UserInterface& parent);

private:
    void Update() final;
    void HandleInput(const Input::Event& event) final;
    void SetHelp(bool on) final;

    lv_obj_t* m_speedometer_label {nullptr};
    lv_obj_t* m_speedometer_unit_label {nullptr};

    lv_obj_t* m_power_label {nullptr};

    lv_obj_t* m_battery_label {nullptr};
    lv_obj_t* m_battery_value_label {nullptr};
    lv_obj_t* m_battery_value_unit_label {nullptr};

    lv_obj_t* m_temperature_label {nullptr};
    lv_obj_t* m_temperature_value_label {nullptr};
    lv_obj_t* m_temperature_value_unit_label {nullptr};

    lv_obj_t* m_trip_distance_label {nullptr};
    lv_obj_t* m_trip_distance_value_label {nullptr};
    lv_obj_t* m_trip_time_label {nullptr};

    lv_obj_t* m_consumption_label {nullptr};

    lv_obj_t* m_range_label {nullptr};
    lv_obj_t* m_range_value {nullptr};
};
