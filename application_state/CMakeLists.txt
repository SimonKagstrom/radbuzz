set(TOP ${CMAKE_CURRENT_LIST_DIR}/../)
include(${TOP}/external/libmaelir/cmake/generate_application_state.cmake)

set(RADBUZZ_APPLICATION_STATE
    "${TOP}/application_state/parameters.yml"
    # TODO: Should come from libmaelir
    "${TOP}/external/libmaelir/application_state/parameters_common.yml"
    "${TOP}/external/libmaelir/application_state/application_state_common.yml"
    "${TOP}/application_state/application_state.yml"
)
list(JOIN RADBUZZ_APPLICATION_STATE ";" RADBUZZ_APPLICATION_STATE_STR)

generate_application_state(radbuzz_application_state "${RADBUZZ_APPLICATION_STATE_STR}")


target_link_libraries(radbuzz_application_state
PUBLIC
    wgs84_to_osm_point
    libmaelir_application_state
)

target_include_directories(radbuzz_application_state
PUBLIC
    include
)
