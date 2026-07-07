add_library(ble_injector EXCLUDE_FROM_ALL
    ble_injector.cc
)

target_include_directories(ble_injector
PUBLIC
    include
)

target_link_libraries(ble_injector
PUBLIC
    radbuzz_interface
)
