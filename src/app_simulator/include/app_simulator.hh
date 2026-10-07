#pragma once

#include "application_state.hh"
#include "bresenham.hh"
#include "hal/i_gps.hh"
#include "messages.hh"
#include "pooled_thread_base.hh"
#include "post_office.hh"
#include "wgs84_to_osm_point.hh"

#include <random>
#include <unordered_set>
#include <vector>

class AppSimulator : public PooledThreadBase
{
public:
    explicit AppSimulator(ApplicationState& app_state, PostOffice<MSG::AllMessages>& post_office);

private:
    std::optional<milliseconds> OnActivation() final;

    void SetupStreetOrder();

    ApplicationState& m_application_state;
    PostOffice<MSG::AllMessages>& m_post_office;

    std::random_device m_random_device;
    std::linear_congruential_engine<uint32_t, 48271, 0, 2147483647> m_random_engine {
        m_random_device()};

    // State data
    std::vector<const char*> m_streets;
    int32_t m_distance_left {0};

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

    os::TimerHandle m_overheated_timer;
    uint8_t m_soc {100};
    int8_t m_soc_delta {-1};

    TurnSymbol m_turn_symbol {TurnSymbol::kDestination};
};
