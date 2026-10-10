#pragma once

#include "hal/i_wifi_client.hh"

#include <esp_wifi.h>

class WifiClientEsp32 : public hal::IWifiClient
{
public:
    WifiClientEsp32();

private:
    void Enable() final;
    void Disable() final;
    void StartScan() final;
    std::vector<std::string> GetScanResult() final;
    void Connect(const char* ssid, const char* password) final;
    void Disconnect() final;
    std::unique_ptr<ListenerCookie> AttachListener(std::function<void(Event)> on_event) final;

    static void
    EventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    std::function<void(Event)> m_on_event {[](auto) { /* NOP */ }};
    esp_event_handler_instance_t m_event_data {};
    esp_event_handler_instance_t m_ip_event_data {};
};
