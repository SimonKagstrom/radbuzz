#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "hal/i_wifi_client.hh"
#include "messages.hh"
#include "post_office.hh"
#include "wifi_connector.hh"
#include "wifi_state_machine_base.hh"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

struct WifiHandlerStateData : WifiStateMachineData
{
    struct Scanning : WifiStateMachineData::Scanning
    {
        // By the UI, so finished also when moving
        bool requested {false};
    };

    struct Idle : WifiStateMachineData::Idle
    {
        os::TimerHandle timer;
    };

    struct Connecting : WifiStateMachineData::Connecting
    {
        // Created on entry (state data must be default-constructible)
        std::optional<WifiConnector> connector;
        // From the last evaluation, for Exit()
        WifiConnector::Result result {WifiConnector::Result::kPending};
    };
};

class WifiHandler : public os::BaseThread, public WifiStateMachineBase<WifiHandlerStateData>
{
public:
    WifiHandler(ApplicationState& state,
                PostOffice<MSG::AllMessages>& post_office,
                Filesystem& filesystem,
                hal::IWifiClient& wifi_client);

private:
    static constexpr auto kMovementTime = 1min;
    static constexpr auto kIdleTime = 10s;

    // WifiStateMachineBase, see wifi_state_machine.md
    On::Next Evaluate(On&) final;
    Scanning::Next Evaluate(Scanning& state) final;
    Idle::Next Evaluate(Idle&) final;
    Connecting::Next Evaluate(Connecting& state) final;
    ConnectFailed::Next Evaluate(ConnectFailed&) final;
    Connected::Next Evaluate(Connected&) final;
    LostConnection::Next Evaluate(LostConnection&) final;
    Off::Next Evaluate(Off&) final;

    void Enter(On&) final;
    void Enter(Scanning& state) final;
    void Enter(Idle& state) final;
    void Enter(Connecting& state) final;
    void Exit(Connecting& state) final;
    void Enter(Connected&) final;
    void Exit(Connected&) final;
    void Enter(Off&) final;

    void OnTransition(State from, State to) final;

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    // Add/update networks from SSID.TXT to the configuration
    void ReadSsidFile();

    // Send the last scan result to the UI (and others)
    void SendScanResult();

    // Store a requested network first among the known networks. False if it's invalid
    bool StoreRequestedNetwork(const MSG::wifi_connect& request);

    // Disconnect, or stop an ongoing connection attempt
    void Disconnect();

    // A scan or connect request from the UI, which both start with a scan
    bool ScanOrConnectRequested() const;

    // The first configured network found in the last scan, which hasn't failed here
    std::optional<WifiSsidNetwork> FindKnownNetwork() const;

    // Moving/standing still for at least kMovementTime
    bool MovingForAWhile() const;
    bool StandingStillForAWhile() const;


    ApplicationState& m_state;
    PostOffice<MSG::AllMessages>& m_post_office;
    Filesystem& m_filesystem;
    hal::IWifiClient& m_wifi_client;

    ListenerCookie m_state_listener;
    ApplicationState::PartialReadOnlyCache<AS::is_moving, AS::configuration> m_state_cache;
    ListenerCookie m_wifi_listener;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;

    // Set from the wifi client callback
    std::atomic_bool m_link_up {false};
    std::atomic_bool m_link_down_event {false};
    std::atomic_bool m_scan_done_event {false};

    std::optional<std::vector<hal::IWifiClient::AccessPoint>> m_scan_result;

    // The networks which failed at this location
    std::vector<std::string> m_failed_ssids;

    // From MSG::start_wifi_scan/wifi_connect, until the next scan is started
    bool m_scan_requested {false};
    bool m_connect_requested {false};

    // Restarted when is_moving changes
    os::TimerHandle m_movement_timer;
};
