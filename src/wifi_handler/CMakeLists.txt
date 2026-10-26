generate_state_machine(wifi_state_machine wifi_state_machine.md)
generate_state_machine(wifi_connect_state_machine wifi_connect_state_machine.md)

add_library(wifi_handler EXCLUDE_FROM_ALL
    wifi_connector.cc
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
    post_office
    radbuzz_messages
    wifi_connect_state_machine
    wifi_state_machine
)
