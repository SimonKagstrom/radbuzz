cmake_minimum_required (VERSION 3.21)
project (radbuzz_unittest LANGUAGES CXX C ASM)

include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        1da23a3e8119ec5cce4f9388e91b065e20bf06f5 # v2.4.12
)
FetchContent_Declare(
  trompeloeil
  GIT_REPOSITORY https://github.com/rollbear/trompeloeil.git
  GIT_TAG        eaeb89c1ce9d354b0ba7eb921fd5712cdbd78adf # v49
)
FetchContent_MakeAvailable(doctest trompeloeil)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Enable sanitizers in debug builds
add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined -g)
include_directories(../../qt/lvgl_setup)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

enable_testing()

add_subdirectory(../.. radbuzz)
add_subdirectory(../../external/libmaelir/test libmaelir_test)

add_library(filesystem_implementation ALIAS mock_filesystem)
add_library(httpd_client_implementation ALIAS mock_httpd_client)
add_library(display_properties ALIAS display_800x800)

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
