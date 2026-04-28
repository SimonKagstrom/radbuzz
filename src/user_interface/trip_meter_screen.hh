#pragma once

#include "user_interface.hh"

#include <vector>

class TripMeterScreen : public UserInterface::ScreenBase
{
public:
    explicit TripMeterScreen(UserInterface& parent);

private:
    enum class StatValueKind
    {
        kConsumedWh,
        kRegeneratedWh,
        kTripAverageWhPerKm,
        kTripMaxSpeed,
        kTripDistance,

        kValueCount,
    };

    struct StatRow
    {
        const char* label_text {nullptr};
        const char* unit_text {nullptr};
        StatValueKind value_kind {StatValueKind::kConsumedWh};
        lv_obj_t* label {nullptr};
        lv_obj_t* value {nullptr};
        lv_obj_t* unit {nullptr};
    };

    void Update() final;
    void HandleInput(Input::Event event) final;

    std::vector<StatRow> m_stat_rows;

    lv_style_t m_style_bar_bg;
    lv_style_t m_style_bar_indicator;
    lv_obj_t* m_consumption_bar {nullptr};
};
