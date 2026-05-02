add_library(wifi_client_host EXCLUDE_FROM_ALL
    wifi_client_host.cc
)

target_include_directories(wifi_client_host
PUBLIC
    include
)

target_link_libraries(wifi_client_host
PUBLIC
    radbuzz_interface
)
