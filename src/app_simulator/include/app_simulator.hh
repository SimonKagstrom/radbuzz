#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "ble_server_host.hh"
#include "bresenham.hh"
#include "hal/i_gps.hh"
#include "wgs84_to_osm_point.hh"

#include <random>
#include <unordered_set>
#include <vector>

class AppSimulator : public os::BaseThread
{
public:
    AppSimulator(ApplicationState& app_state, BleServerHost& ble_server);

private:
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

    uint8_t m_target_speed {10};

    std::unique_ptr<ListenerCookie> m_state_listener;
    ApplicationState::PartialReadOnlyCache<AS::demo_mode> m_state_cache;

    std::vector<Point> m_demo_route;
    Point m_current_point {0, 0, kDefaultZoom};
    std::vector<Point>::iterator m_next_point;

    Bresenham<Point> m_bresenham;
    Bresenham<Point>::Iterator m_bresenham_iterator;

    uint16_t m_target_heading;
    uint16_t m_heading;
};
