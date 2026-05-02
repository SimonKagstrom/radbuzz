#include "wifi_handler.hh"

#include <ranges>

WifiHandler::WifiHandler(ApplicationState& state,
                         Filesystem& filesystem,
                         hal::IWifiClient& wifi_client)
    : m_state(state)
    , m_filesystem(filesystem)
    , m_wifi_client(wifi_client)
    , m_state_listener(m_state.AttachListener<AS::configuration, AS::is_moving>(GetSemaphore()))
{
}

void
WifiHandler::OnStartup()
{
    auto ssid_data = m_filesystem.ReadFile("SSID.TXT");

    WifiSsidData parsed_ssid_data {};
    if (ssid_data)
    {
        std::stringstream ssid_stream(reinterpret_cast<const char*>(ssid_data->data()));

        while (true)
        {
            std::string ssid, password;
            std::getline(ssid_stream, ssid);
            if (ssid == "")
            {
                break;
            }
            std::getline(ssid_stream, password);
            if (password == "")
            {
                break;
            }

            parsed_ssid_data.networks.push_back({ssid, password});
        }
    }
    auto conf = m_state.CheckoutReadonly().Get<AS::configuration>();
    if (!std::ranges::equal(parsed_ssid_data.networks, conf->wifi_ssid_data.networks))
    {
        // Update the configuration with the new SSID data (invalidate conf)
        conf = nullptr;
        auto ps = m_state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().wifi_ssid_data = parsed_ssid_data;
    }

    m_wifi_listener = m_wifi_client.AttachListener([this](auto event) {
        auto rw = m_state.CheckoutReadWrite();

        printf("Wifi event: %s\n",
               event == hal::IWifiClient::Event::kConnected ? "Connected" : "Disconnected");
        if (event == hal::IWifiClient::Event::kConnected)
        {
            rw.Set<AS::wifi_connected>(true);
        }
        else if (event == hal::IWifiClient::Event::kDisconnected)
        {
            rw.Set<AS::wifi_connected>(false);
        }
    });

    auto ssids = m_wifi_client.Scan();
    // Might have been modified by the FS read above, so re-read
    conf = m_state.CheckoutReadonly().Get<AS::configuration>();
    for (const auto& [ssid, password] : conf->wifi_ssid_data.networks)
    {
        if (std::ranges::find(ssids, ssid) != ssids.end())
        {
            m_wifi_client.Connect(ssid.c_str(), password.c_str());
            return;
        }
    }
}

std::optional<milliseconds>
WifiHandler::OnActivation()
{
    // NYI
    return std::nullopt;
}
