#include "ble_handler.hh"

BleHandler::BleHandler(hal::IBleServer& server,
                       hal::IBleClient& client,
                       ApplicationState& state,
                       ImageCache& cache)
    : m_server(server)
    , m_state(state)
    , m_image_cache(cache)
    , m_gadget_bridge_transport(server, m_gadget_bridge_protocol)
{
    // Add a black image for the invalid icon
    auto invalid_data = std::make_unique<uint8_t[]>(kImageByteSize);
    memset(invalid_data.get(), 0, kImageByteSize);
    m_image_cache.Insert(kInvalidIconHash,
                         kImageWidth,
                         kImageHeight,
                         {static_cast<const uint8_t*>(invalid_data.get()), kImageByteSize});


    m_king_shark_handler = std::make_unique<BleKingSharkHandler>(*this, client);
}

void
BleHandler::OnStartup()
{
    m_ble_poller = StartTimer(20ms, [this]() {
        m_server.PollEvents();

        return 20ms;
    });

    m_connection_listener = m_server.AttachConnectionListener([this](bool connected) {
        auto rw = m_state.CheckoutReadWrite();

        rw.Set<AS::bluetooth_connected>(connected);
        if (connected == false)
        {
            rw.Set<AS::navigation_active>(false);
        }
    });

    m_server.Start();

    m_client_startup = StartTimer(1s, [this]() {
        m_king_shark_handler->OnStartup();
        return std::nullopt;
    });
}

std::optional<milliseconds>
BleHandler::OnActivation()
{
    m_king_shark_handler->Update();

    return std::nullopt;
}
