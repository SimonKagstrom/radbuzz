include(FetchContent)

FetchContent_Declare(
  chivo_fonts
  GIT_REPOSITORY https://github.com/Omnibus-Type/Chivo.git
  GIT_TAG        dc61c468d79781eb5183426e88e844af16cdc3e5
)
FetchContent_MakeAvailable(chivo_fonts)

set(GENERAL_FONT "${chivo_fonts_SOURCE_DIR}/fonts/Chivo/ttf/Chivo-Medium.ttf")
set(NUMBERS_FONT "${chivo_fonts_SOURCE_DIR}/fonts/Chivo Mono/ttf/ChivoMono-Medium.ttf")

# Font with size 22, containing all ASCII characters (0x20...0x7F) and some extra symbols.
convert_font(radbuzz_font_22
    ${GENERAL_FONT}
    22
    # ASCII, Latin-1 (Western European) and Latin Extended-A (Central European, Baltic,
    # Turkish), plus Romanian ȘșȚț
    0x20-0x7F,0xA0-0x17F,0x218-0x21B
    # Typographic quotes/dashes, bullet, ellipsis, euro
    "–—‘’‚“”„•…€"
)

# Big font with numbers plus a few extras
convert_font(radbuzz_font_60
    ${GENERAL_FONT}
    60
    0x20,0x30-0x39
    "%:./-"
)

# Big font for only numbers
convert_font(radbuzz_font_40
    ${NUMBERS_FONT}
    40
    0x30-0x39
    "%:./-"
)

# Huge font for only numbers
convert_font(radbuzz_font_120
    ${NUMBERS_FONT}
    120
    0x30-0x39
    ""
)

convert_font(radbuzz_turn_symbols_60
    ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/MaterialSymbolsOutlined_FILL_GRAD_opsz_wght.ttf
    60
    # kDestination kStraight kTurnRight kTurnLeft kTurnSharpRight kTurnSharpLeft kTurnSlightRight kTurnSlightLeft kUturnRight kUturnLeft kRampRight kRampLeft kForkRight kForkLeft kRoundaboutRight kRoundaboutLeft kMerge (phone)
    0xe0c8,0xeb95,0xebab,0xeba6,0xebaa,0xeba7,0xeb9a,0xeba4,0xeba2,0xeba1,0xeb96,0xeb9c,0xebac,0xeba0,0xeba3,0xeb99,0xeb98,0xe0b0
    ""
)

convert_symbols(radbuzz_symbols_40
    40
    # Battery, GPS, WiFi, BLE, home, pause, warning, call, drive ("email") (see lv_symbol_def.h), " "
    0xf240-0xf244,0xf124,0xf1eb,0xf293,0xf015,0xf04C,0xf071,0xf095,0xf01C,0x50,0x20
)

convert_symbols(radbuzz_symbols_22
    22
    # Home
    0xf015
)

# Small font
convert_font(radbuzz_font_16
    ${GENERAL_FONT}
    16
    0x20-0x7F
    "W+-%"
)

# Small font, only numbers
convert_font(radbuzz_numbers_font_16
    ${NUMBERS_FONT}
    16
    0x30-0x39
    "Wmih+-% "
)

add_library(user_interface EXCLUDE_FROM_ALL
    digital_speedometer_widget.cc
    incoming_call_screen.cc
    map_screen.cc
    message_box.cc
    navigation_widget.cc
    settings_menu_screen.cc
    side_pane.cc
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
    ui_menu
    radbuzz_messages
PRIVATE
    bresenham
    cohen_sutherland
    radbuzz_numbers_font_16_library
    radbuzz_font_22_library
    radbuzz_symbols_40_library
    radbuzz_font_40_library
    radbuzz_font_60_library
    radbuzz_font_120_library
    radbuzz_symbols_22_library
    radbuzz_turn_symbols_60_library
)
