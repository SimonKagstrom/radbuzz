# Font with size 22, containing all ASCII characters (0x20...0x7F) and some extra symbols.
convert_font(radbuzz_font_22
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    22
    0x20-0x7F
    "°ÅÄÖåäöÁéÉíÍñÑóÓúÚüÜ•"
)

# Big font with numbers plus a few extras
convert_font(radbuzz_font_60
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    60
    0x20,0x30-0x39
    "%:./-"
)

# Big font for only numbers
convert_font(radbuzz_font_40
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    40
    0x30-0x39
    "W "
)

# Huge font for only numbers
convert_font(radbuzz_font_120
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    120
    0x30-0x39
    ""
)

convert_symbols(radbuzz_symbols_40
    40
    # Battery, GPS, WiFi, BLE, home, pause, warning (see lv_symbol_def.h), " "
    0xf240-0xf244,0xf124,0xf1eb,0xf293,0xf015,0xf04C,0xf071,0x50,0x20
)

convert_symbols(radbuzz_symbols_22
    22
    # Home
    0xf015
)

# Small font
convert_font(radbuzz_font_16
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    16
    0x20-0x7F
    "W+-%"
)

add_library(user_interface EXCLUDE_FROM_ALL
    digital_speedometer_widget.cc
    map_screen.cc
    settings_menu_screen.cc
    speech_bubble.cc
    speedometer_only_screen.cc
    trip_meter_screen.cc
    user_interface.cc
)

target_include_directories(user_interface
PUBLIC
    include
)

target_link_libraries(user_interface
PUBLIC
    application_state
    base_thread
    image_cache
    lvgl
    input
    tile_cache
    trip_computer
    radbuzz_font_16_library
    radbuzz_font_22_library
    radbuzz_symbols_40_library
    radbuzz_font_40_library
    radbuzz_font_60_library
    radbuzz_font_120_library
    radbuzz_symbols_22_library
    ui_menu
PRIVATE
    bresenham
    cohen_sutherland
)
