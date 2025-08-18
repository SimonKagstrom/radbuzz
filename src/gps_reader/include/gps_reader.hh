#pragma once

#include "base_thread.hh"
#include "hal/i_gps.hh"

#include <array>
#include <atomic>
#include <etl/vector.h>

#include "wgs84_to_osm_point.hh"

struct GpsData
{
    GpsPosition position;
    Point pixel_position;

    float speed;
    float heading;

    // Add time, height, etc.
};

class IGpsPort
{
public:
    virtual ~IGpsPort() = default;

    void AwakeOn(os::binary_semaphore& semaphore)
    {
        DoAwakeOn(&semaphore);
    }

    void DisableWakeup()
    {
        DoAwakeOn(nullptr);
    }

    virtual std::optional<GpsData> Poll() = 0;

protected:
    virtual void DoAwakeOn(os::binary_semaphore* semaphore) = 0;
};


class GpsReader : public os::BaseThread
{
public:
    explicit GpsReader(hal::IGps& gps);

    std::unique_ptr<IGpsPort> AttachListener();

private:
    class GpsPortImpl;

    std::optional<milliseconds> OnActivation() final;

    void Reset();

    hal::IGps& m_gps;

    etl::vector<GpsPortImpl*, 8> m_listeners;
    std::array<std::atomic_bool, 8> m_stale_listeners;

    std::optional<GpsPosition> m_position;
    std::optional<float> m_speed;
    std::optional<float> m_heading;
};
