include(external/libmaelir/cmake/generate_state_machine.cmake)
include(external/libmaelir/cmake/git_version.cmake)

git_version_header(git_version_header ${CMAKE_CURRENT_SOURCE_DIR})

# The common lv_conf.h for all platforms. A cache variable, since LVGL is added from libmaelir
# (not below this directory), and LVGL declares it as a cache variable itself
set(LV_BUILD_CONF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_config CACHE PATH "Directory with lv_conf.h" FORCE)

add_subdirectory(application_state)
add_subdirectory(external)
add_subdirectory(messages)
add_subdirectory(src)
