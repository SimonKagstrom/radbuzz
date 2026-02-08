add_library(ble_handler EXCLUDE_FROM_ALL
    ble_handler.cc
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

