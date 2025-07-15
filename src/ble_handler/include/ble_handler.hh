#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_ble_server.hh"
#include "image_cache.hh"

constexpr auto kImageWidth = 64;
constexpr auto kImageHeight = 62;
constexpr auto kImageByteSize = (kImageWidth * kImageHeight) / 8;

constexpr auto kServiceUuid = "ec91d7ab-e87c-48d5-adfa-cc4b2951298a";

constexpr auto kChaSettings = "9d37a346-63d3-4df6-8eee-f0242949f59f";
constexpr auto kChaNav = "0b11deef-1563-447f-aece-d3dfeb1c1f20";
constexpr auto kChaNavTbtIcon = "d4d8fcca-16b2-4b8e-8ed5-90137c44a8ad";
constexpr auto kChaNavTbtIconDesc = "d63a466e-5271-4a5d-a942-a34ccdb013d9";
constexpr auto kChaGpsSpeed = "98b6073a-5cf3-4e73-b6d3-f8e05fa018a9";

class BleHandler : public os::BaseThread
{
public:
    BleHandler(hal::IBleServer& server, ApplicationState& state, ImageCache& cache);

private:
    // From BaseThread
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void OnChaNav(std::span<const uint8_t> data);
    void OnIcon(std::span<const uint8_t> data);

    os::TimerHandle m_ble_poller;
    hal::IBleServer& m_server;
    ApplicationState& m_state;
    ImageCache& m_image_cache;
};
