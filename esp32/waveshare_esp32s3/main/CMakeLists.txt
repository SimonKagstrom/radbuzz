# Register the component
idf_component_register(SRCS "main.cc" INCLUDE_DIRS "")

# ... but use regular CMake for the rest
target_link_libraries(${COMPONENT_TARGET}
    idf::freertos
    idf::spi_flash
    idf::esp_partition
    idf::vfs
    idf::fatfs
    idf::sdmmc
    idf::esp_wifi
    ble_handler
    ble_server_esp32
    button_debouncer
    rotary_encoder
    gpio_esp32
    uart_gps_esp32
    nvm_esp32
    os_esp32
    st7701_display_esp32
    uart_esp32
    buzz_handler
    user_interface
    tile_cache
    gps_reader
)
