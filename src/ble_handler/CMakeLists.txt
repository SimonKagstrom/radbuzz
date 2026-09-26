# "The DOWNLOAD_EXTRACT_TIMESTAMP option was not given and policy..."
cmake_policy(SET CMP0135 NEW)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)


add_library(ble_handler EXCLUDE_FROM_ALL
    ble_handler.cc
    ble_king_shark_handler.cc
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

