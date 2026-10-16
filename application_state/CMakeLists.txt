set(TOP ${CMAKE_CURRENT_LIST_DIR}/../)
include(${TOP}/external/libmaelir/cmake/generate_application_state.cmake)

generate_application_state(radbuzz_application_state
    BASE
        libmaelir_application_state
    FILES
        parameters.yml
        application_state.yml
)

target_link_libraries(radbuzz_application_state
PUBLIC
    wgs84_to_osm_point
)

target_include_directories(radbuzz_application_state
PUBLIC
    include
)
