#pragma once

#include "application_state.hh"

#include <esp_wifi.h>

class WifiClientEsp32
{
public:
    WifiClientEsp32(ApplicationState& app_state);

    void Start(const char* ssid, const char* password);
    void Stop();

    static void
    EventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

private:
    ApplicationState& m_app_state;
    esp_event_handler_instance_t m_event_data {};
    esp_event_handler_instance_t m_ip_event_data {};
};
