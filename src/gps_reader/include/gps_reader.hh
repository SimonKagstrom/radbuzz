#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_gps.hh"

#include <array>
#include <atomic>
#include <etl/vector.h>


class GpsReader : public os::BaseThread
{
public:
    explicit GpsReader(ApplicationState &application_state,
        hal::IGps& gps);

private:
    std::optional<milliseconds> OnActivation() final;

    void Reset();

    ApplicationState &m_application_state;
    hal::IGps& m_gps;


    std::optional<GpsPosition> m_position;
    std::optional<float> m_speed;
    std::optional<float> m_heading;
};
