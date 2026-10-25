set(OSM_API_KEY_FILENAME "${CMAKE_CURRENT_SOURCE_DIR}/../../osm_api_key.txt")

set(OSM_API_KEY "")
if(EXISTS "${OSM_API_KEY_FILENAME}")
    file(READ "${OSM_API_KEY_FILENAME}" OSM_API_KEY)
    string(STRIP "${OSM_API_KEY}" OSM_API_KEY) # Remove whitespace/newlines
endif()


set(TILE_STYLE_SAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../resources/tile_styles")

# Sample tiles of each style (from Stockholm, at the default zoom)
library_from_data(${TILE_STYLE_SAMPLES_DIR}/atlas.png tile_style_atlas)
library_from_data(${TILE_STYLE_SAMPLES_DIR}/cycle.png tile_style_cycle)
library_from_data(${TILE_STYLE_SAMPLES_DIR}/landscap.png tile_style_landscape)
library_from_data(${TILE_STYLE_SAMPLES_DIR}/mobatlas.png tile_style_mobile_atlas)
library_from_data(${TILE_STYLE_SAMPLES_DIR}/neighbor.png tile_style_neighbourhood)

add_library(tile_cache EXCLUDE_FROM_ALL
    png_decoder.cc
    tile_cache.cc
    tile_regions.cc
    tile_styles.cc
)

target_include_directories(tile_cache
PUBLIC
    include
)

target_link_libraries(tile_cache
PUBLIC
    https_client
    base_thread
    application_state
    painter
    filesystem_interface
    wgs84_to_osm_point
    filesystem_implementation
    radbuzz_messages
PRIVATE
    pngdec
    tile_style_atlas_library
    tile_style_cycle_library
    tile_style_landscape_library
    tile_style_mobile_atlas_library
    tile_style_neighbourhood_library
)

target_compile_definitions(tile_cache
PUBLIC
    OSM_API_KEY="${OSM_API_KEY}"
)
