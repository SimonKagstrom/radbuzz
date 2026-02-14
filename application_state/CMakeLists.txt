include(../external/libmaelir/cmake/generate_application_state.cmake)

generate_application_state(radbuzz_application_state
    "${CMAKE_CURRENT_LIST_DIR}/parameters.yml;${CMAKE_CURRENT_LIST_DIR}/application_state.yml"
)
