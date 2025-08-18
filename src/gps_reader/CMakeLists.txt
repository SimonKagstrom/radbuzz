add_library(gps_reader EXCLUDE_FROM_ALL
    gps_reader.cc
)

target_include_directories(gps_reader
PUBLIC
    include
)

target_link_libraries(gps_reader
PUBLIC
    base_thread
    wgs84_to_osm_point
)
