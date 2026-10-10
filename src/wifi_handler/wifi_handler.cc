#include "wifi_handler.hh"

#include <ranges>
#include <sstream>

WifiHandler::WifiHandler(ApplicationState& state,
                         Filesystem& filesystem,
                         hal::IWifiClient& wifi_client)
    : m_state(state)
    , m_filesystem(filesystem)
    , m_wifi_client(wifi_client)
    , m_state_listener(m_state.AttachListener<AS::configuration, AS::is_moving>(GetSemaphore()))
    , m_state_cache(m_state)
{
}

void
WifiHandler::OnStartup()
{
    ReadSsidFile();

    m_movement_timer = StartTimer(kMovementTime);

    // Called from the wifi driver context, so only record the event and wake up the thread
    m_wifi_listener = m_wifi_client.AttachListener([this](auto event) {
        switch (event)
        {
        case hal::IWifiClient::Event::kConnected:
            m_link_up = true;
            break;
        case hal::IWifiClient::Event::kDisconnected:
            m_link_up = false;
            m_link_down_event = true;
            break;
        case hal::IWifiClient::Event::kScanDone:
            m_scan_done_event = true;
            break;
        }

        Awake();
    });
}

std::optional<milliseconds>
WifiHandler::OnActivation()
{
    m_state_cache.Pull().OnChanged<AS::is_moving>(
        [this]() { m_movement_timer = StartTimer(kMovementTime); });

    if (m_scan_done_event.exchange(false))
    {
        m_scan_result = m_wifi_client.GetScanResult();
    }

    RunStateMachine();

    return std::nullopt;
}

void
WifiHandler::ReadSsidFile()
{
    auto ssid_data = m_filesystem.ReadFile("SSID.TXT");

    WifiSsidData parsed_ssid_data {};
    if (ssid_data)
    {
        const std::string ssid_text(reinterpret_cast<const char*>(ssid_data->data()),
                                    ssid_data->size());
        std::stringstream ssid_stream(ssid_text);

        printf("Read SSID.TXT:\n%s\n", ssid_text.c_str());
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
            printf("Parsed SSID: %s, Password: %s\n", ssid.c_str(), password.c_str());

            parsed_ssid_data.networks.push_back({ssid, password});
        }
    }

    auto ps = m_state.CheckoutPartialSnapshot<AS::configuration>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    for (auto& [ssid, password] : parsed_ssid_data.networks)
    {
        auto it = std::ranges::find_if(conf.wifi_ssid_data.networks, [ssid](const auto& network) {
            return network.ssid == ssid;
        });
        if (it != conf.wifi_ssid_data.networks.end())
        {
            // Update password (if changed)
            it->password = password;
        }
        else
        {
            // Add new
            conf.wifi_ssid_data.networks.push_back({ssid, password});
        }
    }
}

std::optional<WifiSsidNetwork>
WifiHandler::FindKnownNetwork() const
{
    if (!m_scan_result)
    {
        return std::nullopt;
    }

    auto conf = m_state.Get<AS::configuration>();
    for (const auto& network : conf->wifi_ssid_data.networks)
    {
        if (std::ranges::find(*m_scan_result, network.ssid) != m_scan_result->end())
        {
            return network;
        }
    }

    return std::nullopt;
}

bool
WifiHandler::MovingForAWhile() const
{
    return m_state_cache.Get<AS::is_moving>() && m_movement_timer->IsExpired();
}

bool
WifiHandler::StandingStillForAWhile() const
{
    return !m_state_cache.Get<AS::is_moving>() && m_movement_timer->IsExpired();
}

void
WifiHandler::OnTransition(State from, State to)
{
    printf("Wifi: %s -> %s\n", StateName(from).data(), StateName(to).data());
}


// On
void
WifiHandler::Enter(On&)
{
    m_wifi_client.Enable();
}

WifiHandler::On::Next
WifiHandler::Evaluate(On&)
{
    return State::kScanning;
}


// Scanning
void
WifiHandler::Enter(Scanning&)
{
    m_scan_result = std::nullopt;
    m_scan_done_event = false;
    m_connect_retries = 0;

    m_wifi_client.StartScan();
}

WifiHandler::Scanning::Next
WifiHandler::Evaluate(Scanning&)
{
    if (MovingForAWhile())
    {
        return State::kOff;
    }
    if (!m_scan_result)
    {
        return kStay;
    }
    if (FindKnownNetwork())
    {
        return State::kConnect;
    }

    return State::kIdle;
}


// Idle
void
WifiHandler::Enter(Idle& state)
{
    state.timer = StartTimer(kIdleTime);
}

WifiHandler::Idle::Next
WifiHandler::Evaluate(Idle& state)
{
    if (state.timer->IsExpired())
    {
        return State::kScanning;
    }

    return kStay;
}


// Connect
void
WifiHandler::Enter(Connect&)
{
    m_link_down_event = false;

    if (auto network = FindKnownNetwork(); network)
    {
        m_wifi_client.Connect(network->ssid.c_str(), network->password.c_str());
    }
    else
    {
        // Removed from the configuration since the scan, treat as a failed connection
        m_link_down_event = true;
    }
}

WifiHandler::Connect::Next
WifiHandler::Evaluate(Connect&)
{
    if (m_link_up)
    {
        return State::kConnected;
    }
    if (m_link_down_event)
    {
        if (m_connect_retries < kMaxConnectRetries)
        {
            return State::kRetryConnect;
        }

        return State::kScanning;
    }

    return kStay;
}


// RetryConnect
void
WifiHandler::Enter(RetryConnect& state)
{
    m_connect_retries++;
    state.timer = StartTimer(kRetryConnectTime);
}

WifiHandler::RetryConnect::Next
WifiHandler::Evaluate(RetryConnect& state)
{
    if (state.timer->IsExpired())
    {
        return State::kConnect;
    }

    return kStay;
}


// Connected
void
WifiHandler::Enter(Connected&)
{
    m_state.CheckoutReadWrite().Set<AS::wifi_connected>(true);
}

void
WifiHandler::Exit(Connected&)
{
    m_state.CheckoutReadWrite().Set<AS::wifi_connected>(false);
}

WifiHandler::Connected::Next
WifiHandler::Evaluate(Connected&)
{
    if (!m_link_up)
    {
        return State::kLostConnection;
    }

    return kStay;
}


// LostConnection
WifiHandler::LostConnection::Next
WifiHandler::Evaluate(LostConnection&)
{
    if (MovingForAWhile())
    {
        return State::kOff;
    }
    if (StandingStillForAWhile())
    {
        return State::kScanning;
    }

    return kStay;
}


// Off
void
WifiHandler::Enter(Off&)
{
    m_wifi_client.Disable();
    m_link_up = false;
}

WifiHandler::Off::Next
WifiHandler::Evaluate(Off&)
{
    if (StandingStillForAWhile())
    {
        return State::kOn;
    }

    return kStay;
}
