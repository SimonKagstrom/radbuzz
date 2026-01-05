# Register the component
idf_component_register(SRCS "main.cc" INCLUDE_DIRS "")

# ... but use regular CMake for the rest
target_link_libraries(${COMPONENT_TARGET}
PRIVATE
    idf::freertos
    idf::spi_flash
    idf::esp_partition
    idf::vfs
    idf::fatfs
    idf::sdmmc
    wifi_client_esp32
    ble_handler
    ble_server_esp32
    button_debouncer
    rotary_encoder
    gpio_esp32
    app_simulator
    uart_gps_esp32
    i2c_gps_esp32
    nvm_esp32
    os_esp32
    pm_esp32
    uart_esp32
    buzz_handler
    user_interface
    tile_cache
    jd9365_display_esp32
    gps_reader
)

# TODO: Add pthread etc above

# If this component depends on other components - be it ESP-IDF or project-specific ones - enumerate those in the double-quotes below, separated by spaces
# Note that pthread should always be there, or else STD will not work
set(RUST_DEPS "pthread" "driver" "vfs")
# Here's a non-minimal, reasonable set of ESP-IDF components that one might want enabled for Rust:
#set(RUST_DEPS "pthread" "esp_http_client" "esp_http_server" "espcoredump" "app_update" "esp_serial_slave_link" "nvs_flash" "spi_flash" "esp_adc_cal" "mqtt")


set(RUST_TARGET "riscv32imafc-esp-espidf")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_BUILD_TYPE "debug")
    set(CARGO_BUILD_ARG "")
else()
    set(CARGO_BUILD_TYPE "release")
    set(CARGO_BUILD_ARG "--release")
endif()

set(CARGO_BUILD_STD_ARG -Zbuild-std=std,panic_abort)

set(ESP_RUSTFLAGS "--cfg espidf_time64 --cfg=force_disable_atomic64 ")

# TMP
set(CARGO_PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../rust/wifi_handler")
set(CARGO_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(CARGO_TARGET_DIR "${CARGO_BUILD_DIR}/target")

set(RUST_INCLUDE_DIR "${CARGO_TARGET_DIR}")
set(RUST_STATIC_LIBRARY "${CARGO_TARGET_DIR}/${RUST_TARGET}/${CARGO_BUILD_TYPE}/libwifi_handler.a")

# if this component uses CBindGen to generate a C header, uncomment the lines below and adjust the header name accordingly
#set(RUST_INCLUDE_HEADER "${RUST_INCLUDE_DIR}/RustApi.h")
#set_source_files_properties("${RUST_INCLUDE_HEADER}" PROPERTIES GENERATED true)

idf_build_get_property(sdkconfig SDKCONFIG)
idf_build_get_property(idf_path IDF_PATH)

ExternalProject_Add(
    project_rust_target
    PREFIX "${CARGO_PROJECT_DIR}"
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
        cargo clean --target ${RUST_TARGET} --target-dir ${CARGO_TARGET_DIR}
    USES_TERMINAL_BUILD true
    BUILD_COMMAND ${CMAKE_COMMAND} -E env
        "CXXFLAGS_riscv32imafc_esp_espidf=-march=rv32imafc -mabi=ilp32f"
        CARGO_CMAKE_BUILD_INCLUDES=$<TARGET_PROPERTY:${COMPONENT_TARGET},INCLUDE_DIRECTORIES>
        CARGO_CMAKE_BUILD_LINK_LIBRARIES=$<TARGET_PROPERTY:${COMPONENT_TARGET},LINK_LIBRARIES>

        CARGO_CMAKE_BUILD_COMPILER=${CMAKE_C_COMPILER}
        RUSTFLAGS=${ESP_RUSTFLAGS}
        CROSS_COMPILE=riscv32-esp-elf
        MCU=${CONFIG_IDF_TARGET}
        cargo build --target ${RUST_TARGET} --target-dir ${CARGO_TARGET_DIR} ${CARGO_BUILD_ARG} ${CARGO_BUILD_STD_ARG}
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
    TMP_DIR "${CARGO_BUILD_DIR}/tmp"
    STAMP_DIR "${CARGO_BUILD_DIR}/stamp"
    DOWNLOAD_DIR "${CARGO_BUILD_DIR}"
    SOURCE_DIR "${CARGO_PROJECT_DIR}"
    BINARY_DIR "${CARGO_PROJECT_DIR}"
    INSTALL_DIR "${CARGO_BUILD_DIR}"
    BUILD_BYPRODUCTS
        "${RUST_INCLUDE_HEADER}"
        "${RUST_STATIC_LIBRARY}"
)

add_library(library_rust_target STATIC IMPORTED)
set_target_properties(library_rust_target PROPERTIES
    IMPORTED_LOCATION "${RUST_STATIC_LIBRARY}"
)
add_dependencies(library_rust_target project_rust_target)

target_include_directories(${COMPONENT_TARGET} PUBLIC "${RUST_INCLUDE_DIR}")
target_link_libraries(${COMPONENT_TARGET} PRIVATE library_rust_target)
