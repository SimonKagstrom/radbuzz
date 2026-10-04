add_library(ble_server_host EXCLUDE_FROM_ALL
    ble_server_host.cc
)

target_include_directories(ble_server_host
PUBLIC
    include
)

target_link_libraries(ble_server_host
PUBLIC
    radbuzz_interface
)
