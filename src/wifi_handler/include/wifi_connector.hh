#pragma once

#include "configuration_settings.hh"
#include "hal/i_wifi_client.hh"
#include "timer_manager.hh"
#include "wifi_connect_state_machine_base.hh"

#include <atomic>
#include <string>

struct WifiConnectorStateData : WifiConnectStateMachineData
{
    struct RetryConnect : WifiConnectStateMachineData::RetryConnect
    {
        os::TimerHandle timer;
    };
};

// Connects to one network, with retries, for the Connecting state of the wifi handler. See
// wifi_connect_state_machine.md
class WifiConnector : public WifiConnectStateMachineBase<WifiConnectorStateData>
{
public:
    enum class Result
    {
        kPending,
        kSucceeded,
        kFailed,
    };

    // The link flags are set from the wifi client callback (by the wifi handler)
    WifiConnector(hal::IWifiClient& wifi_client,
                  os::TimerManager& timer_manager,
                  const std::atomic_bool& link_up,
                  std::atomic_bool& link_down_event,
                  WifiSsidNetwork network);

    // Stops an ongoing connection attempt, e.g., when leaving for a requested network
    ~WifiConnector() override;

    // Run the state machine
    Result EvaluateConnection();

    const std::string& Ssid() const;

private:
    static constexpr auto kRetryConnectTime = 5s;
    static constexpr auto kMaxConnectRetries = 3;

    // WifiConnectStateMachineBase, see wifi_connect_state_machine.md
    Connect::Next Evaluate(Connect&) final;
    RetryConnect::Next Evaluate(RetryConnect& state) final;
    Succeeded::Next Evaluate(Succeeded&) final;
    Failed::Next Evaluate(Failed&) final;

    void Enter(Connect&) final;
    void Enter(RetryConnect& state) final;

    void OnTransition(State from, State to) final;


    hal::IWifiClient& m_wifi_client;
    os::TimerManager& m_timer_manager;
    const std::atomic_bool& m_link_up;
    std::atomic_bool& m_link_down_event;

    const WifiSsidNetwork m_network;
    unsigned m_retries {0};
};
