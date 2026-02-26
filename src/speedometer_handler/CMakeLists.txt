add_library(speedometer_handler EXCLUDE_FROM_ALL
    speedometer_handler.cc
)

target_include_directories(speedometer_handler
PUBLIC
    include
)

target_link_libraries(speedometer_handler
PUBLIC
    base_thread
    application_state
)
