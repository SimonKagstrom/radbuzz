#pragma once

#include "application_state.hh"

namespace trip
{

inline auto
AverageConsumption(ApplicationState& state, const auto& trip_start)
{
    // For the trip, not the odometer
    const uint32_t trip_distance_m = state.Get<AS::trip_distance>();

    const float total_wh_consumed = state.Get<AS::wh_consumed>();
    const float trip_wh_consumed = std::max(0.0f, total_wh_consumed - trip_start.start_wh_consumed);

    return trip_distance_m > 0 ? (trip_wh_consumed * 1000.0f) / trip_distance_m : 0.0f;
}

} // namespace trip
