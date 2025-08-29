add_library(wifi_client_esp32 EXCLUDE_FROM_ALL
    wifi_client_esp32.cc
)

target_include_directories(wifi_client_esp32
PUBLIC
    include
)

target_link_libraries(wifi_client_esp32
PUBLIC
    idf::esp_wifi
    idf::nvs_flash
    application_state
)
