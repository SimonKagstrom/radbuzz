cmake_minimum_required (VERSION 3.21)

include(ExternalProject)

include_directories(../lvgl_setup)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

# Include for ESP-IDF build system functions
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
#include($ENV{IDF_PATH}/tools/cmake/idf.cmake)

project (radbuzz_esp32p4 LANGUAGES CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

add_subdirectory(.. target)
add_subdirectory(../../external/libmaelir/esp32/jd9365_esp32 libmaelir_target_display)
add_library(filesystem_implementation ALIAS std_filesystem)
add_library(display_properties ALIAS display_800x800)

target_link_libraries(lvgl
PRIVATE
    idf::driver
    idf::esp_driver_ppa
    idf::esp_mm
)
