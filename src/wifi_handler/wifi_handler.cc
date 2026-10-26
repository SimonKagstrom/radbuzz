#include "wifi_handler.hh"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <sstream>

namespace
{

// 32-bit FNV-1a, to detect changes to SSID.TXT
uint32_t
Fnv1aHash(std::span<const std::byte> data)
{
    uint32_t hash = 2166136261u;

    for (auto byte : data)
    {
        hash ^= std::to_integer<uint32_t>(byte);
        hash *= 16777619u;
    }

    return hash;
}

// A line without the line ending (also \r\n, from Windows). False at the end of the file
bool
ReadLine(std::istream& stream, std::string& line)
{
    if (!std::getline(stream, line))
    {
        return false;
    }
    if (line.ends_with('\r'))
    {
        line.pop_back();
    }

    return true;
}

} // namespace

WifiHandler::WifiHandler(ApplicationState& state,
                         PostOffice<MSG::AllMessages>& post_office,
                         Filesystem& filesystem,
                         hal::IWifiClient& wifi_client)
    : m_state(state)
    , m_post_office(post_office)
    , m_filesystem(filesystem)
    , m_wifi_client(wifi_client)
    , m_state_listener(m_state.AttachListener<AS::configuration, AS::is_moving>(GetSemaphore()))
    , m_state_cache(m_state)
    , m_mailbox(m_post_office.Subscribe<MSG::start_wifi_scan, MSG::wifi_connect>(GetNotifier()))
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
    m_state_cache.Pull()
        .OnNewValue<AS::is_moving>([this](auto is_moving) {
            m_movement_timer = StartTimer(kMovementTime);

            // Probably somewhere else when stopping again, so try all networks there
            if (is_moving)
            {
                m_failed_ssids.clear();
            }
        })
        .OnChangedValue<AS::configuration>([this](const auto& old_conf, const auto& new_conf) {
            // E.g., a changed password
            if (old_conf.wifi_ssid_data != new_conf.wifi_ssid_data)
            {
                m_failed_ssids.clear();
            }
        });

    // A scan in progress also covers the requests (and finds a requested network)
    const auto scanning = CurrentState() == State::kScanning;
    m_mailbox->Collect()
        .On<MSG::start_wifi_scan>([this, scanning]() { m_scan_requested |= !scanning; })
        .On<MSG::wifi_connect>([this, scanning](auto request) {
            if (StoreRequestedNetwork(*request))
            {
                m_connect_requested |= !scanning;
            }
        });

    if (m_scan_done_event.exchange(false))
    {
        m_scan_result = m_wifi_client.GetScanResult();
        SendScanResult();
    }

    RunStateMachine();

    return std::nullopt;
}

void
WifiHandler::ReadSsidFile()
{
    auto ssid_data = m_filesystem.ReadFile("SSID.TXT");
    if (!ssid_data)
    {
        return;
    }

    // Only import a changed file, so that networks forgotten (or changed) on the device stay
    // that way. The same SD card can still be moved between boards to set them up.
    const auto hash = Fnv1aHash(*ssid_data);
    if (hash == m_state.Get<AS::configuration>()->ssid_file_hash)
    {
        printf("SSID.TXT unchanged since the last import, ignoring it\n");
        return;
    }

    WifiSsidData parsed_ssid_data {};
    const std::string ssid_text(reinterpret_cast<const char*>(ssid_data->data()),
                                ssid_data->size());
    std::stringstream ssid_stream(ssid_text);

    printf("Read SSID.TXT:\n%s\n", ssid_text.c_str());
    while (true)
    {
        std::string ssid, password;
        if (!ReadLine(ssid_stream, ssid) || ssid.empty())
        {
            break;
        }
        // An empty password line is an open network, but a missing one is a broken file
        if (!ReadLine(ssid_stream, password))
        {
            break;
        }

        parsed_ssid_data.networks.push_back({ssid, password});
    }

    auto ps = m_state.CheckoutPartialSnapshot<AS::configuration>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    conf.ssid_file_hash = hash;
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

void
WifiHandler::SendScanResult()
{
    MSG::wifi_scan_result result;

    for (const auto& ap : *m_scan_result)
    {
        // Hidden networks have no SSID, and the same network can be seen from several APs.
        // Keep the first one, which is the strongest if the client sorts by RSSI.
        if (ap.ssid.empty() ||
            std::ranges::find(result.networks, ap.ssid, &MSG::WifiScanEntry::ssid) !=
                result.networks.end())
        {
            continue;
        }

        result.networks.push_back(
            {ap.ssid, ap.rssi, ap.secured ? MSG::WifiType::kSecured : MSG::WifiType::kOpen});
    }

    m_post_office.Send(std::move(result));
}

bool
WifiHandler::StoreRequestedNetwork(const MSG::wifi_connect& request)
{
    // The limits of the wifi standard (and the ESP32 driver). A 64 character password is a
    // hex key instead of a passphrase
    if (request.ssid.empty() || request.ssid.size() > 32 || request.password.size() > 64)
    {
        printf("Wifi: Ignoring invalid network '%s'\n", request.ssid.c_str());
        return false;
    }

    auto ps = m_state.CheckoutPartialSnapshot<AS::configuration>();
    auto& networks = ps.GetWritableReference<AS::configuration>().wifi_ssid_data.networks;

    // First, so that it's preferred over the other known networks
    std::erase_if(networks, [&request](const auto& network) { return network.ssid == request.ssid; });
    networks.insert(networks.begin(), {request.ssid, request.password});

    // E.g., retyped after a wrong password. Also cleared by the configuration change, but
    // the scan result might be handled before that
    std::erase(m_failed_ssids, request.ssid);

    return true;
}

void
WifiHandler::Disconnect()
{
    m_wifi_client.Disconnect();
    // Not connected until a new connection is made, even if the event is late
    m_link_up = false;
}

bool
WifiHandler::ScanOrConnectRequested() const
{
    return m_scan_requested || m_connect_requested;
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
        if (std::ranges::find(m_failed_ssids, network.ssid) != m_failed_ssids.end())
        {
            continue;
        }
        if (std::ranges::find(*m_scan_result,
                              network.ssid,
                              &hal::IWifiClient::AccessPoint::ssid) != m_scan_result->end())
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
WifiHandler::Enter(Scanning& state)
{
    m_scan_result = std::nullopt;
    m_scan_done_event = false;

    // This scan handles the requests
    state.requested = ScanOrConnectRequested();
    m_scan_requested = false;
    m_connect_requested = false;

    m_wifi_client.StartScan();
}

WifiHandler::Scanning::Next
WifiHandler::Evaluate(Scanning& state)
{
    if (!state.requested && MovingForAWhile())
    {
        return State::kOff;
    }
    if (!m_scan_result)
    {
        return kStay;
    }
    if (FindKnownNetwork())
    {
        return State::kConnecting;
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
    if (state.timer->IsExpired() || ScanOrConnectRequested())
    {
        return State::kScanning;
    }

    return kStay;
}


// Connecting
void
WifiHandler::Enter(Connecting& state)
{
    // Found by the state before, unless another thread changed the configuration in between
    if (auto network = FindKnownNetwork(); network)
    {
        state.connector.emplace(
            m_wifi_client, GetTimerManager(), m_link_up, m_link_down_event, *network);
    }
}

WifiHandler::Connecting::Next
WifiHandler::Evaluate(Connecting& state)
{
    if (!state.connector)
    {
        return State::kConnectFailed;
    }

    state.result = state.connector->EvaluateConnection();
    if (state.result == WifiConnector::Result::kSucceeded)
    {
        return State::kConnected;
    }
    if (m_connect_requested)
    {
        return State::kScanning;
    }
    if (state.result == WifiConnector::Result::kFailed)
    {
        return State::kConnectFailed;
    }

    return kStay;
}

void
WifiHandler::Exit(Connecting& state)
{
    // An ongoing attempt is stopped when the connector is destroyed
    if (state.connector && state.result == WifiConnector::Result::kFailed)
    {
        // Skip it until somewhere else, or until the configuration has changed
        m_failed_ssids.push_back(state.connector->Ssid());
    }
    else if (state.connector && state.result == WifiConnector::Result::kSucceeded)
    {
        // The last chance to know the network. Cleared when leaving Connected
        m_state.Set<AS::connected_wifi_ssid>(state.connector->Ssid());
    }
}


// ConnectFailed
WifiHandler::ConnectFailed::Next
WifiHandler::Evaluate(ConnectFailed&)
{
    // Connect requests are handled by Connecting, so none here

    // From the same scan, since a new one would probably not find anything else
    if (FindKnownNetwork())
    {
        return State::kConnecting;
    }

    return State::kIdle;
}


// Connected
void
WifiHandler::Enter(Connected&)
{
    m_state.Set<AS::wifi_connected>(true);
}

void
WifiHandler::Exit(Connected&)
{
    m_state.Set<AS::wifi_connected>(false);
    m_state.Set<AS::connected_wifi_ssid>(std::string());

    // Still connected, i.e., left for a requested network
    if (m_link_up)
    {
        Disconnect();
    }
}

WifiHandler::Connected::Next
WifiHandler::Evaluate(Connected&)
{
    if (!m_link_up)
    {
        return State::kLostConnection;
    }
    if (m_connect_requested)
    {
        return State::kScanning;
    }
    if (m_scan_requested)
    {
        // Scanning works while connected, so no need to leave the state
        m_scan_requested = false;
        m_wifi_client.StartScan();
    }

    return kStay;
}


// LostConnection
WifiHandler::LostConnection::Next
WifiHandler::Evaluate(LostConnection&)
{
    if (ScanOrConnectRequested())
    {
        return State::kScanning;
    }
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
    if (StandingStillForAWhile() || ScanOrConnectRequested())
    {
        return State::kOn;
    }

    return kStay;
}
