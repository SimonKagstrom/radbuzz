#pragma once

#include "application_state.hh"

namespace battery
{

inline auto
FullPackWh(auto conf)
{
    // Estimate Wh left from configured pack size
    constexpr float kNominalCellVoltageV = 3.7f;
    const float pack_nominal_voltage_v =
        static_cast<float>(conf->battery_cell_series) * kNominalCellVoltageV;

    return static_cast<uint32_t>(conf->battery_amp_hours) * pack_nominal_voltage_v;
}

} // namespace battery