add_subdirectory(ble_handler)
add_subdirectory(user_interface)

add_library(buzzbar_interface INTERFACE)

target_include_directories(buzzbar_interface INTERFACE
    include
)

target_link_libraries(buzzbar_interface INTERFACE
    libmaelir_interface
)
