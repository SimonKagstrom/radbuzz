include (libmaelir/cmake/convert_font.cmake)

# Uses zephyr normally, so add it manually here
add_library(vesc_can_sdk EXCLUDE_FROM_ALL
    vesc_can_sdk/vesc_buffer.c
    vesc_can_sdk/vesc_can_sdk.c
    vesc_can_sdk/vesc_crc.c
)

target_include_directories(vesc_can_sdk
PUBLIC
    vesc_can_sdk/include)
