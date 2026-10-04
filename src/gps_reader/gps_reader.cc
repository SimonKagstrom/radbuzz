#include "gps_reader.hh"

#include "wgs84_to_osm_point.hh"

#include <cassert>
#include <etl/queue_spsc_atomic.h>
#include <span>


GpsReader::GpsReader(ApplicationState& application_state, hal::IGps& gps)
    : m_application_state(application_state)
    , m_gps(gps)
{
    auto stockholm = Wgs84ToOsmPoint({59.3293, 18.0686}, kDefaultZoom);

    m_application_state.CheckoutReadWrite().Set<AS::pixel_position>(*stockholm);
}

std::optional<milliseconds>
GpsReader::OnActivation()
{
    auto data = m_gps.WaitForData(GetSemaphore());

    if (!data)
    {
        // No data, wait for the next activation
        return std::nullopt;
    }

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

    if (m_application_state.Get<AS::demo_mode>())
    {
        // No resetting of the state done while in demo mode
        m_gps_timeout_timer = nullptr;
        m_gps_data_timeout_timer = nullptr;

        return std::nullopt;
    }

    auto qw = m_application_state
                  .CheckoutQueuedWriter<AS::position, AS::pixel_position, AS::gps_status>();
    if (m_application_state.Get<AS::gps_status>() == GpsStatus::kSilent)
    {
        // We have data, so at least NoFix should be set
        qw.Set<AS::gps_status>(GpsStatus::kNoFix);
        m_gps_data_timeout_timer = StartTimer(5s, [this]() {
            if (m_application_state.Get<AS::demo_mode>() == false)
            {
                m_application_state.CheckoutReadWrite().Set<AS::gps_status>(
                    GpsStatus::kSilent);
            }
            return std::nullopt;
        });
    }

    if (!m_position || !m_speed || !m_heading)
    {
        // Wait for the complete data
        return std::nullopt;
    }

    GpsData mangled;

    mangled.position = *m_position;
    mangled.heading = *m_heading;
    mangled.speed = *m_speed * 1.852f; // Convert from knots to km/h

    // Disable, and restart again
    qw.Set<AS::position>(mangled);
    if (auto pixel_pos = Wgs84ToOsmPoint(mangled.position, kDefaultZoom); pixel_pos)
    {
        qw.Set<AS::pixel_position>(*pixel_pos);
    }
    qw.Set<AS::gps_status>(GpsStatus::kPositionValid);

    m_gps_timeout_timer = StartTimer(10s, [this]() {
        m_application_state.CheckoutReadWrite().Set<AS::gps_status>(GpsStatus::kNoFix);
        return std::nullopt;
    });
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
