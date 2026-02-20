#include "gps_reader.hh"

#include "wgs84_to_osm_point.hh"

#include <cassert>
#include <etl/queue_spsc_atomic.h>
#include <span>


GpsReader::GpsReader(ApplicationState &application_state, hal::IGps& gps)
    : m_application_state(application_state)
    , m_gps(gps)
{
}

std::optional<milliseconds>
GpsReader::OnActivation()
{
    auto data = m_gps.WaitForData(GetSemaphore());

    if (data->position)
    {
        m_position = data->position;
    }
    if (data->speed)
    {
        m_speed = data->speed;
    }
    if (data->heading)
    {
        m_heading = data->heading;
    }

    if (!m_position || !m_speed || !m_heading)
    {
        // Wait for the complete data
        return std::nullopt;
    }

    GpsData mangled;

    mangled.position = *m_position;
    mangled.heading = *m_heading;
    mangled.speed = *m_speed;
    if (auto pixel_pos = Wgs84ToOsmPoint(*m_position, 15); pixel_pos)
    {
        mangled.pixel_position = *pixel_pos;
    }
    else
    {
        // If conversion fails, set to zero
        mangled.pixel_position = Point {0, 0};
    }


    m_application_state.CheckoutReadWrite().Set<AS::position>(mangled);
    Reset();

    return std::nullopt;
}


void
GpsReader::Reset()
{
    m_position = std::nullopt;
    m_speed = std::nullopt;
    m_heading = std::nullopt;
}
