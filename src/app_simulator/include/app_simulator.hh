#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "ble_server_host.hh"
#include "hal/i_gps.hh"
#include "wgs84_to_osm_point.hh"

#include <random>
#include <unordered_set>
#include <vector>

class AppSimulator : public os::BaseThread
{
public:
    AppSimulator(ApplicationState& app_state, BleServerHost& ble_server);

    hal::IGps& GetSimulatedGps();

private:
    class SimulatedGps : public hal::IGps
    {
    public:
        void NextPoint(const Point& point);

    private:
        std::optional<hal::RawGpsData> WaitForData(os::binary_semaphore& semaphore) final;

        Point m_current_point {0, 0};
        os::binary_semaphore m_data_semaphore {0};
    };

    std::optional<milliseconds> OnActivation() final;

    void SetupStreetOrder();

    ApplicationState& m_application_state;
    BleServerHost& m_ble_server;

    std::random_device m_random_device;
    std::linear_congruential_engine<uint32_t, 48271, 0, 2147483647> m_random_engine {
        m_random_device()};

    // State data
    std::vector<const char*> m_streets;
    int32_t m_distance_left {0};
    uint8_t m_current_image {0};

    std::unordered_set<uint32_t> m_cached_images;

    SimulatedGps m_gps;
    Point m_current_point {0, 0};

    uint8_t m_target_speed {10};
};
