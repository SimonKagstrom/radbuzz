cmake_minimum_required (VERSION 3.21)
project (radbuzz_unittest LANGUAGES CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Enable sanitizers in debug builds
add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined -g)
include_directories(../../qt/lvgl_setup)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

find_package(etl REQUIRED)
find_package(trompeloeil REQUIRED)
find_package(doctest REQUIRED)

enable_testing()

add_subdirectory(../.. radbuzz)
add_subdirectory(../../external/libmaelir/test libmaelir_test)

add_library(filesystem_implementation ALIAS mock_filesystem)
add_library(httpd_client_implementation ALIAS mock_httpd_client)

add_executable(ut
    main.cc
    test_ble_handler.cc
    test_tile_cache.cc
)

target_link_libraries(ut
    ble_handler
    tile_cache
    os_unittest
    mock_filesystem
    doctest::doctest
    trompeloeil::trompeloeil
)
add_test(NAME unittest COMMAND ut)
