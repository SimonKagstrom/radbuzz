add_library(ble_server_esp32 EXCLUDE_FROM_ALL
    ble_server_esp32.cc
)

target_include_directories(ble_server_esp32
PUBLIC
    include
)

target_link_libraries(ble_server_esp32
PUBLIC
    idf::bt
    idf::nvs_flash
    radbuzz_interface
)
