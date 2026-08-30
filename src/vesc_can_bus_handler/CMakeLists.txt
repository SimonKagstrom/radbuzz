add_library(vesc_can_bus_handler EXCLUDE_FROM_ALL
    vesc_can_bus_handler.cc
)

target_include_directories(vesc_can_bus_handler
PUBLIC
    include
)

target_link_libraries(vesc_can_bus_handler
PUBLIC
    base_thread
    application_state
PRIVATE
    vesc_can_sdk
)
