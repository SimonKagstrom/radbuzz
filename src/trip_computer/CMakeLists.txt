add_library(trip_computer EXCLUDE_FROM_ALL
    trip_computer.cc
)

target_include_directories(trip_computer
PUBLIC
    include
)

target_link_libraries(trip_computer
PUBLIC
    job_pool_thread
    application_state
    trip_log
    wgs84_to_osm_point
    radbuzz_messages
)
