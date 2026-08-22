#pragma once

#include "trip_computer.hh"
#include "user_interface.hh"

#include <etl/vector.h>

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

    void DrawHistogramLines(lv_layer_t* layer);

    lv_obj_t* m_speedometer_box {nullptr};

    lv_obj_t* m_speedometer_label {nullptr};
    lv_obj_t* m_speedometer_unit_label {nullptr};

    etl::vector<lv_obj_t*, TripComputer::kNumberOfRecentEntries> m_recent_entry_bars {};
    std::array<lv_obj_t*, 2> m_recent_entry_labels {};

    lv_obj_t* m_current_histogram_bar_label {nullptr};

    Datum m_battery;
    Datum m_temperature;
    Datum m_trip_distance;
    Datum m_trip_time;
    Datum m_range;
    Datum m_power;
    Datum m_consumption;

    lv_obj_t* m_consumption_label {nullptr};
};
