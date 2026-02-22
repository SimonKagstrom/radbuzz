add_library(app_simulator EXCLUDE_FROM_ALL
    app_simulator.cc
)

target_include_directories(app_simulator
PUBLIC
    include
)

target_link_libraries(app_simulator
PUBLIC
    application_state
    ble_server_host
    base_thread
    wgs84_to_osm_point
PRIVATE
    ble_handler
)
