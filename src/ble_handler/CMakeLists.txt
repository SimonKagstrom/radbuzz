# "The DOWNLOAD_EXTRACT_TIMESTAMP option was not given and policy..."
cmake_policy(SET CMP0135 NEW)

include(FetchContent)

# SYSTEM, to silence warnings from its headers. 3.12.0 uses std::is_trivial, which GCC 15 warns
# about since it's deprecated in C++26 (fixed upstream, but not yet in a release)
FetchContent_Declare(json
    SYSTEM
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)


generate_state_machine(king_shark_packet_protocol_state_machine king_shark_packet_protocol_state_machine.md)

add_library(ble_handler EXCLUDE_FROM_ALL
    ble_handler.cc
    ble_king_shark_handler.cc
    gadget_bridge_protocol.cc
    gadget_bridge_transport.cc
    king_shark_packet_protocol.cc
)

target_include_directories(ble_handler
PUBLIC
    include
)

target_link_libraries(ble_handler
PUBLIC
    base_thread
    image_cache
    application_state
    nlohmann_json::nlohmann_json
    post_office
    radbuzz_messages
    king_shark_packet_protocol_state_machine
PRIVATE
    radbuzz_interface
)

# For unit tests
add_library(ble_handler_private INTERFACE)
target_include_directories(ble_handler_private INTERFACE
    .
)
target_link_libraries(ble_handler_private
INTERFACE
    ble_handler
)

