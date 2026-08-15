#pragma once

#include "user_interface.hh"

class SpeedometerOnlyScreen : public UserInterface::ScreenBase
{
public:
    struct Datum
    {
        lv_obj_t* description_label {nullptr};
        lv_obj_t* value_label {nullptr};
        lv_obj_t* value_unit_label {nullptr};
    };

    explicit SpeedometerOnlyScreen(UserInterface& parent);

private:
    void Update() final;
    void HandleInput(const Input::Event& event) final;
    void SetHelp(bool on) final;

    Datum RightAligned(const char* label_text,
                       const char* value_text,
                       const char* unit_text,
                       lv_align_t alignment,
                       Point offset);
    Datum LeftAligned(const char* label_text,
                      const char* value_text,
                      const char* unit_text,
                      lv_align_t alignment,
                      Point offset);
    Datum CenterAligned(const char* label_text,
                        const char* value_text,
                        const char* unit_text,
                        lv_align_t alignment,
                        Point offset);

    lv_obj_t* m_speedometer_box {nullptr};

    lv_obj_t* m_speedometer_label {nullptr};
    lv_obj_t* m_speedometer_unit_label {nullptr};

    Datum m_battery;
    Datum m_temperature;
    Datum m_trip_distance;
    Datum m_range;
    Datum m_power;

    lv_obj_t* m_consumption_label {nullptr};
};
