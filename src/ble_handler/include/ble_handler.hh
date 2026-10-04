#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "ble_king_shark_handler.hh"
#include "gadget_bridge_protocol.hh"
#include "gadget_bridge_transport.hh"
#include "hal/i_ble_client.hh"
#include "hal/i_ble_server.hh"
#include "image_cache.hh"
#include "messages.hh"
#include "post_office.hh"

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
               PostOffice<MSG::AllMessages>& post_office,
               ImageCache& cache);

private:
    // From BaseThread
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    os::TimerHandle m_ble_poller;
    os::TimerHandle m_client_startup;
    hal::IBleServer& m_server;
    ApplicationState& m_state;
    PostOffice<MSG::AllMessages>& m_post_office;
    ImageCache& m_image_cache;

    std::unique_ptr<ListenerCookie> m_connection_listener;

    GadgetBridgeProtocol m_gadget_bridge_protocol {
        GetTimerManager(), m_state, m_post_office, GetSemaphore()};
    GadgetBridgeTransport m_gadget_bridge_transport;

    std::unique_ptr<BleKingSharkHandler> m_king_shark_handler;
};
