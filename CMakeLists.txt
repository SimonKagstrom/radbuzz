include(external/libmaelir/cmake/generate_state_machine.cmake)
include(external/libmaelir/cmake/git_version.cmake)

git_version_header(git_version_header ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(application_state)
add_subdirectory(external)
add_subdirectory(messages)
add_subdirectory(src)
