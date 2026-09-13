add_library(temperature_monitor EXCLUDE_FROM_ALL
    temperature_monitor.cc
)

target_include_directories(temperature_monitor
PUBLIC
    include
)

target_link_libraries(temperature_monitor
PUBLIC
    base_thread
    application_state
)
