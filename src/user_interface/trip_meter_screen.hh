#pragma once

#include "user_interface.hh"

class TripMeterScreen : public UserInterface::ScreenBase
{
public:
    TripMeterScreen(UserInterface& parent);

private:
    void Update() final;

    lv_obj_t* m_battery_millivolts_label {nullptr};
};
