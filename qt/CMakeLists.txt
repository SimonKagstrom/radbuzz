cmake_minimum_required(VERSION 3.21)
project(radbuzz_qt LANGUAGES CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Qt setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Ignore some irritating warnings on MacOS
if(APPLE)
    add_link_options(-Wl,-no_warn_duplicate_libraries)

    # https://stackoverflow.com/questions/4929255/building-static-libraries-on-mac-using-cmake-and-gcc
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

# Enable sanitizers in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -g)
    add_link_options(-fsanitize=address,undefined -g)
else()
    add_compile_options(-g)
    add_link_options(-g)
endif()

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(etl REQUIRED)

add_subdirectory(.. radbuzz)
add_subdirectory(../external/libmaelir/qt libradbuzz_qt)
add_subdirectory(ble_server_host)


add_executable(radbuzz_qt
    simulator_main.cc
    simulator_mainwindow.ui
    simulator_mainwindow.cc
)

target_link_libraries(radbuzz_qt
    display_qt
    os_qt
    app_simulator
    user_interface
    ble_handler
    ble_server_host
    lvgl
    Qt6::Widgets
)
