include(${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/cmake/generate_state_machine.cmake)
generate_state_machine(wifi_state_machine wifi_state_machine.md)

add_library(wifi_handler EXCLUDE_FROM_ALL
    wifi_handler.cc
)

target_include_directories(wifi_handler
PUBLIC
    include
)

target_link_libraries(wifi_handler
PUBLIC
    base_thread
    application_state
    radbuzz_interface
    filesystem
    wifi_state_machine
)
