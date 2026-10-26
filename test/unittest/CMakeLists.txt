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

set(TOP ${CMAKE_CURRENT_LIST_DIR}/../../)

add_subdirectory(${TOP} radbuzz)

generate_application_state(radbuzz_unittest_application_state
    BASE
        radbuzz_application_state
    FILES
        unittest_parameters.yml
)

add_library(generated_application_state ALIAS radbuzz_unittest_application_state)
add_library(messages ALIAS radbuzz_messages)

# After the aliases above, so that the libmaelir tests use the radbuzz state and messages (which
# include the common ones) instead of defining their own
add_subdirectory(${TOP}/external/libmaelir/test/unittest libmaelir_unittests)

# PNGdec does unaligned accesses on purpose
target_compile_options(pngdec_zlib PRIVATE -fno-sanitize=alignment)
target_compile_options(pngdec PRIVATE -fno-sanitize=alignment)

add_library(filesystem_implementation ALIAS mock_filesystem)
add_library(https_client_implementation ALIAS mock_https_client)
if(NOT TARGET display_properties)
    add_library(display_properties ALIAS display_800x800)
endif()



add_executable(unittest_radbuzz
    main.cc
    test_application_state.cc
    test_ble_handler.cc
    test_buzz_handler.cc
    test_clock_string.cc
    test_distance_string.cc
    test_gps_reader.cc
    test_king_shark_packet_protocol.cc
    test_speedometer_handler.cc
    test_storage.cc
    test_tile_cache.cc
    test_tile_regions.cc
    test_tile_styles.cc
    test_trip_computer.cc
    test_trip_log.cc
    test_wifi_handler.cc
    test_wifi_network_list.cc
)

# The UI logic which doesn't need LVGL, kept next to the UI code using it
target_include_directories(unittest_radbuzz
PRIVATE
    ${TOP}/src/user_interface/settings
)

target_link_libraries(unittest_radbuzz
    application_state
    ble_handler_private
    buzz_handler
    mock_filesystem
    os_unittest
    gps_reader
    speedometer_handler
    storage
    tile_cache
    trip_computer
    wifi_handler


    # Compile tests only for now
    vesc_can_bus_handler
    user_interface

    doctest::doctest
    trompeloeil::trompeloeil
)
add_test(NAME unittest_radbuzz COMMAND unittest_radbuzz)
