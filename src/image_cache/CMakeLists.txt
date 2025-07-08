add_library(image_cache EXCLUDE_FROM_ALL
    image_cache.cc
)

target_include_directories(image_cache
PUBLIC
    include
)

target_link_libraries(image_cache
PUBLIC
    radbuzz_interface
    lvgl
)
