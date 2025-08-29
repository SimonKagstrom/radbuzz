// See https://github.com/espressif/esp-idf/blob/v5.5/examples/wifi/getting_started/station/main/station_example_main.c
#include "wifi_client_esp32.hh"

static const char* TAG = "wifi station";


WifiClientEsp32::WifiClientEsp32(ApplicationState& app_state)
    : m_app_state(app_state)
{
}

void
WifiClientEsp32::Start(const char* ssid, const char* password)
{
    // Wifi
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, static_cast<void*>(this), &m_event_data));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &EventHandler, static_cast<void*>(this), &m_ip_event_data));

    wifi_config_t sta_config = {};

    // Setup ssid etc
    strcpy((char*)sta_config.sta.ssid, ssid);
    strcpy((char*)sta_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    ESP_ERROR_CHECK(esp_wifi_start());
}

void
WifiClientEsp32::Stop()
{
}


void
WifiClientEsp32::EventHandler(void* arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data)
{
    static int s_retry_num = 0;

    auto p = static_cast<WifiClientEsp32*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        auto state = p->m_app_state.Checkout();
        state->wifi_connected = false;

        if (s_retry_num < 5)
        {
            esp_wifi_connect();
            s_retry_num++;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        auto state = p->m_app_state.Checkout();

        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        s_retry_num = 0;

        state->wifi_connected = true;
    }
}
