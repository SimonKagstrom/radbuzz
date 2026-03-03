#pragma once

#include "user_interface.hh"

class TripMeterScreen : public UserInterface::ScreenBase
{
public:
    TripMeterScreen(UserInterface& parent);

private:
    void Update() final;
    void HandleInput(hal::IInput::EventType event) final;

    lv_obj_t* m_battery_millivolts_label {nullptr};
};
