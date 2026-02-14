cmake_minimum_required (VERSION 3.21)
project (unittest_radbuzz LANGUAGES CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Enable sanitizers in debug builds
add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined -g)
include_directories(../../qt/lvgl_setup)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

enable_testing()

add_subdirectory(../.. radbuzz)
add_subdirectory(../../external/libmaelir/test/unittest libmaelir_unittests)

add_library(filesystem_implementation ALIAS mock_filesystem)
add_library(httpd_client_implementation ALIAS mock_httpd_client)
add_library(display_properties ALIAS display_800x800)
add_library(generated_application_state ALIAS radbuzz_application_state)

add_executable(unittest_radbuzz
    main.cc
    test_application_state.cc
    test_ble_handler.cc
    test_tile_cache.cc
    test_king_shark_packet_protocol.cc
)

target_link_libraries(unittest_radbuzz
    ble_handler_private
    tile_cache
    os_unittest
    mock_filesystem
    doctest::doctest
    trompeloeil::trompeloeil
)
add_test(NAME unittest_radbuzz COMMAND unittest_radbuzz)
