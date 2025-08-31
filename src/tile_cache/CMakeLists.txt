set(OSM_API_KEY_FILENAME "${CMAKE_CURRENT_SOURCE_DIR}/../../osm_api_key.txt")

file(READ "${OSM_API_KEY_FILENAME}" OSM_API_KEY)
string(STRIP "${OSM_API_KEY}" OSM_API_KEY) # Remove whitespace/newlines


add_library(tile_cache EXCLUDE_FROM_ALL
    tile_cache.cc
)

target_include_directories(tile_cache
PUBLIC
    include
)

target_link_libraries(tile_cache
PUBLIC
    httpd_client
    base_thread
    application_state
    painter
    gps_reader
    filesystem_interface
    wgs84_to_osm_point
    filesystem_implementation
PRIVATE
    pngdec
)

target_compile_definitions(tile_cache
PUBLIC
    OSM_API_KEY="${OSM_API_KEY}"
)
