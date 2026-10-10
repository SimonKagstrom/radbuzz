#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "hal/i_wifi_client.hh"
#include "wifi_state_machine_base.hh"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

struct WifiHandlerStateData : WifiStateMachineData
{
    struct Idle : WifiStateMachineData::Idle
    {
        os::TimerHandle timer;
    };

    struct RetryConnect : WifiStateMachineData::RetryConnect
    {
        os::TimerHandle timer;
    };
};

class WifiHandler : public os::BaseThread, public WifiStateMachineBase<WifiHandlerStateData>
{
public:
    WifiHandler(ApplicationState& state, Filesystem& filesystem, hal::IWifiClient& wifi_client);

private:
    static constexpr auto kMovementTime = 1min;
    static constexpr auto kIdleTime = 10s;
    static constexpr auto kRetryConnectTime = 5s;
    static constexpr auto kMaxConnectRetries = 3;

    // WifiStateMachineBase, see wifi_state_machine.md
    OnNext Evaluate(On&) final;
    ScanningNext Evaluate(Scanning&) final;
    IdleNext Evaluate(Idle&) final;
    ConnectNext Evaluate(Connect&) final;
    RetryConnectNext Evaluate(RetryConnect&) final;
    ConnectedNext Evaluate(Connected&) final;
    LostConnectionNext Evaluate(LostConnection&) final;
    OffNext Evaluate(Off&) final;

    void Enter(On&) final;
    void Enter(Scanning&) final;
    void Enter(Idle& state) final;
    void Enter(Connect&) final;
    void Enter(RetryConnect& state) final;
    void Enter(Connected&) final;
    void Exit(Connected&) final;
    void Enter(Off&) final;

    void OnTransition(State from, State to) final;

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    // Add/update networks from SSID.TXT to the configuration
    void ReadSsidFile();

    // The first configured network found in the last scan
    std::optional<WifiSsidNetwork> FindKnownNetwork() const;

    // Moving/standing still for at least kMovementTime
    bool MovingForAWhile() const;
    bool StandingStillForAWhile() const;


    ApplicationState& m_state;
    Filesystem& m_filesystem;
    hal::IWifiClient& m_wifi_client;

    std::unique_ptr<ListenerCookie> m_state_listener;
    ApplicationState::PartialReadOnlyCache<AS::is_moving> m_state_cache;
    std::unique_ptr<ListenerCookie> m_wifi_listener;

    // Set from the wifi client callback
    std::atomic_bool m_link_up {false};
    std::atomic_bool m_link_down_event {false};
    std::atomic_bool m_scan_done_event {false};

    std::optional<std::vector<std::string>> m_scan_result;
    unsigned m_connect_retries {0};

    // Restarted when is_moving changes
    os::TimerHandle m_movement_timer;
};
