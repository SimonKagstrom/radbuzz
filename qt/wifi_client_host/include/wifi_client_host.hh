#pragma once

#include "hal/i_wifi_client.hh"

class WifiClientHost : public hal::IWifiClient
{
public:
    WifiClientHost() = default;

    // Simulate that the connection to the access point is lost
    void SimulateConnectionLoss();

private:
    void Enable() final;
    void Disable() final;
    void StartScan() final;
    std::vector<std::string> GetScanResult() final;
    void Connect(const char* ssid, const char* password) final;
    void Disconnect() final;
    std::unique_ptr<ListenerCookie> AttachListener(std::function<void(Event)> on_event) final;

    std::function<void(Event)> m_on_event {[](auto) { /* NOP */ }};
};