#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "ble_king_shark_handler.hh"
#include "gadget_bridge_transport.hh"
#include "gadget_bridge_protocol.hh"
#include "hal/i_ble_client.hh"
#include "hal/i_ble_server.hh"
#include "image_cache.hh"

constexpr auto kImageWidth = 64;
constexpr auto kImageHeight = 62;
constexpr auto kImageByteSize = (kImageWidth * kImageHeight) / 8;

class BleHandler : public os::BaseThread
{
public:
    friend class BleKingSharkHandler;
    BleHandler(hal::IBleServer& server,
               hal::IBleClient& client,
               ApplicationState& state,
               ImageCache& cache);

private:
    // From BaseThread
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    os::TimerHandle m_ble_poller;
    os::TimerHandle m_client_startup;
    hal::IBleServer& m_server;
    ApplicationState& m_state;
    ImageCache& m_image_cache;

    std::unique_ptr<ListenerCookie> m_connection_listener;

    GadgetBridgeProtocol m_gadget_bridge_protocol;
    GadgetBridgeTransport m_gadget_bridge_transport;

    std::unique_ptr<BleKingSharkHandler> m_king_shark_handler;
};
