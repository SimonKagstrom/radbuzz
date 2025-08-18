#include "gps_reader.hh"

#include "wgs84_to_osm_point.hh"

#include <cassert>
#include <etl/queue_spsc_atomic.h>
#include <span>

class GpsReader::GpsPortImpl : public IGpsPort
{
public:
    GpsPortImpl(GpsReader* parent, uint8_t index)
        : m_parent(parent)
        , m_index(index)
    {
    }

    ~GpsPortImpl() final
    {
        // Mark this as stale
        m_parent->m_stale_listeners[m_index].store(true);
        m_parent->Awake();
    }

    void PushGpsData(const GpsData& data)
    {
        m_data.push(data);
        if (m_semaphore)
        {
            m_semaphore->release();
        }
    }

private:
    void DoAwakeOn(os::binary_semaphore* semaphore) final
    {
        m_semaphore = semaphore;
    }

    std::optional<GpsData> Poll() final
    {
        std::optional<GpsData> out = std::nullopt;
        GpsData data;

        // Just return the last data, history is not important
        while (m_data.pop(data))
        {
            out = data;
        }

        return out;
    }

    GpsReader* m_parent;
    etl::queue_spsc_atomic<GpsData, 8> m_data;
    os::binary_semaphore* m_semaphore {nullptr};
    const uint8_t m_index;
};


GpsReader::GpsReader(hal::IGps& gps)
    : m_gps(gps)
{
}

std::unique_ptr<IGpsPort>
GpsReader::AttachListener()
{
    assert(!m_listeners.full());

    auto out = std::make_unique<GpsPortImpl>(this, m_listeners.size());
    m_listeners.push_back(out.get());

    return out;
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

    for (auto i = 0u; i < m_stale_listeners.size(); i++)
    {
        if (m_stale_listeners[i].exchange(false))
        {
            m_listeners.erase(m_listeners.begin() + i);
        }
    }

    for (auto& l : m_listeners)
    {
        l->PushGpsData(mangled);
    }

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
