add_library(input_esp32 EXCLUDE_FROM_ALL
    input_esp32.cc
)

target_include_directories(input_esp32
PUBLIC
    include
)

target_link_libraries(input_esp32
PUBLIC
    radbuzz_interface
    gpio_esp32
    rotary_encoder
)
