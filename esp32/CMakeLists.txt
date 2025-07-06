find_package(etl REQUIRED)

add_compile_options(-Wdouble-promotion)

add_subdirectory(ble_server_esp32)

add_subdirectory(.. buzzbar)
add_subdirectory(../external/libmaelir/esp32 libmaelir_esp32)
