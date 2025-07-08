add_library(ble_handler EXCLUDE_FROM_ALL
    ble_handler.cc
)

target_include_directories(ble_handler
PUBLIC
    include
)

target_link_libraries(ble_handler
PUBLIC
    base_thread
    image_cache
)
