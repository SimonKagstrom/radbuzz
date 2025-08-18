add_library(wgs84_to_osm_point EXCLUDE_FROM_ALL
    wgs84_to_osm_point.cc
)

target_include_directories(wgs84_to_osm_point
PUBLIC
    include
)

target_link_libraries(wgs84_to_osm_point
PUBLIC
    libmaelir_interface
)
