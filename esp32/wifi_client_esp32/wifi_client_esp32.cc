// See https://github.com/espressif/esp-idf/blob/v5.5/examples/wifi/getting_started/station/main/station_example_main.c
#include "wifi_client_esp32.hh"

#include <cstring>
#include <esp_wifi.h>

static const char* TAG = "wifi station";


WifiClientEsp32::WifiClientEsp32()
{
    // Wifi
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, static_cast<void*>(this), &m_event_data));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &EventHandler, static_cast<void*>(this), &m_ip_event_data));
}

void
WifiClientEsp32::Enable()
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
}

void
WifiClientEsp32::Disable()
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
}

void
WifiClientEsp32::StartScan()
{
    // Non-blocking, WIFI_EVENT_SCAN_DONE is sent when finished
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_start(nullptr, false));
}

std::vector<std::string>
WifiClientEsp32::GetScanResult()
{
    std::vector<std::string> ssids;

    // Note: Getting the records frees them in the wifi driver, so this only works once per scan
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    std::vector<wifi_ap_record_t> ap_records(ap_count);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records.data()));

    for (const auto& record : ap_records)
    {
        ssids.emplace_back(reinterpret_cast<const char*>(record.ssid));
    }

    return ssids;
}

void
WifiClientEsp32::Connect(const char* ssid, const char* password)
{
    wifi_config_t sta_config = {};

    // Setup ssid etc
    strcpy((char*)sta_config.sta.ssid, ssid);
    strcpy((char*)sta_config.sta.password, password);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void
WifiClientEsp32::Disconnect()
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
}

std::unique_ptr<ListenerCookie>
WifiClientEsp32::AttachListener(std::function<void(Event)> on_event)
{
    m_on_event = on_event;

    return std::make_unique<ListenerCookie>([this]() { m_on_event = [](auto) { /* NOP */ }; });
}

void
WifiClientEsp32::EventHandler(void* arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data)
{
    auto p = static_cast<WifiClientEsp32*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // Connect() explicitly initiates association after setting credentials.
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        auto disconnected = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        printf("Wifi disconnected, reason %d rssi %d\n", disconnected->reason, disconnected->rssi);

        // Retries are handled by the user (the wifi handler)
        p->m_on_event(hal::IWifiClient::Event::kDisconnected);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        p->m_on_event(hal::IWifiClient::Event::kScanDone);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        p->m_on_event(hal::IWifiClient::Event::kConnected);
    }
}
