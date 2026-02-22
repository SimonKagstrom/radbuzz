# Font with size 22, containing all ASCII characters (0x20...0x7F) and some extra symbols.
convert_font(radbuzz_font_22
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    22
    0x20-0x7F
    "°ÅÄÖåäöÁéÉíÍñÑóÓúÚüÜ"
)

# Big font with numbers only
convert_font(radbuzz_font_big
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    60
    0x20,0x30-0x39
    "%.°CV"
)

add_library(user_interface EXCLUDE_FROM_ALL
    map_screen.cc
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
    tile_cache
    radbuzz_font_22_library
    radbuzz_font_big_library
    ui_menu
)
