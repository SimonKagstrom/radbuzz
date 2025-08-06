add_subdirectory(app_simulator)
add_subdirectory(application_state)
add_subdirectory(ble_handler)
add_subdirectory(buzz_handler)
add_subdirectory(image_cache)
add_subdirectory(user_interface)

add_library(radbuzz_interface INTERFACE)

target_include_directories(radbuzz_interface INTERFACE
    include
)

target_link_libraries(radbuzz_interface INTERFACE
    libmaelir_interface
)
