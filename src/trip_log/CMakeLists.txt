add_library(trip_log INTERFACE)

target_include_directories(trip_log
INTERFACE
    include
)

target_link_libraries(trip_log
INTERFACE
    application_state
    wgs84_to_osm_point
)
