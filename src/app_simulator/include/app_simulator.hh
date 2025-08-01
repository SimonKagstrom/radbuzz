#pragma once

#include "base_thread.hh"
#include "ble_server_host.hh"

#include <random>
#include <unordered_set>
#include <vector>

class AppSimulator : public os::BaseThread
{
public:
    AppSimulator(BleServerHost& ble_server);

private:
    std::optional<milliseconds> OnActivation() final;

    BleServerHost& m_ble_server;

    std::random_device m_random_device;
    std::linear_congruential_engine<uint32_t, 48271, 0, 2147483647> m_random_engine {
        m_random_device()};

    // State data
    std::vector<const char*> m_streets;
    int32_t m_distance_left {0};
    uint8_t m_current_image {0};

    std::unordered_set<uint32_t> m_cached_images;
};
