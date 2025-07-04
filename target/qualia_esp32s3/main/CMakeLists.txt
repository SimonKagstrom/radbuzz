# Register the component
idf_component_register(SRCS "main.cc" INCLUDE_DIRS "")

# ... but use regular CMake for the rest
target_link_libraries(${COMPONENT_TARGET}
    idf::freertos
    idf::spi_flash
    idf::esp_partition
    button_debouncer
    rotary_encoder
    gpio_esp32
    nvm_esp32
    os_esp32
    st7701_display_esp32
    uart_esp32
#    user_interface
)
