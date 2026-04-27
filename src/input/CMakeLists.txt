add_library(input EXCLUDE_FROM_ALL
    input.cc
)

target_include_directories(input
PUBLIC
    include
)

target_link_libraries(input
PUBLIC
    base_thread
    radbuzz_interface
    rotary_encoder
)
