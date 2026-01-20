convert_font(radbuzz_font_22
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libmaelir/external/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
    22
    "°ÅÄÖåäöÁéÉíÍñÑóÓúÚüÜ"
)

add_library(user_interface EXCLUDE_FROM_ALL
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
    gps_reader
    radbuzz_font_22_library
    ui_menu
)
