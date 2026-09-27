find_package(Qt6 REQUIRED COMPONENTS Bluetooth)

add_library(ble_server_qt EXCLUDE_FROM_ALL
    ble_server_qt.cc
)

target_include_directories(ble_server_qt
PUBLIC
    include
)

target_link_libraries(ble_server_qt
PUBLIC
    radbuzz_interface
    Qt6::Bluetooth
)
