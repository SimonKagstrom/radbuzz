add_library(trip_compactor EXCLUDE_FROM_ALL
    trip_compactor.cc
)

target_include_directories(trip_compactor
PUBLIC
    include
)

target_link_libraries(trip_compactor
PUBLIC
    filesystem
    job_pool_thread
    post_office
    radbuzz_messages
PRIVATE
    nlohmann_json::nlohmann_json
    trip_log
    trip_recorder
    wgs84_to_osm_point
)
