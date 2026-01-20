add_library(can_bus_handler EXCLUDE_FROM_ALL
    can_bus_handler.cc
)

target_include_directories(can_bus_handler
PUBLIC
    include
)

target_link_libraries(can_bus_handler
PUBLIC
    base_thread
    application_state
PRIVATE
    vesc_can_sdk
)
