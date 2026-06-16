# Font with size 22, containing all ASCII characters (0x20...0x7F) and some extra symbols.
convert_font(radbuzz_font_22
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    22
    0x20-0x7F
    "°ÅÄÖåäöÁéÉíÍñÑóÓúÚüÜ"
)

# Big font with numbers plus a few extras
convert_font(radbuzz_font_60
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    60
    0x20,0x30-0x39
    "%.°CVWh"
)

convert_symbols(radbuzz_symbols_40
    40
    0xf240-0xf244
)

# Small font with numbers
convert_font(radbuzz_font_16
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    16
    0x20,0x30-0x39
    "%"
)

add_library(user_interface EXCLUDE_FROM_ALL
    map_screen.cc
    settings_menu_screen.cc
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
    radbuzz_font_60_library
    ui_menu
PRIVATE
    bresenham
    cohen_sutherland
)
