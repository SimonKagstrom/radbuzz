add_library(traccar_uploader EXCLUDE_FROM_ALL
    traccar_uploader.cc
)

target_include_directories(traccar_uploader
PUBLIC
    include
)

target_link_libraries(traccar_uploader
PUBLIC
    application_state
    base_thread
    filesystem
    https_client
    post_office
    radbuzz_messages
PRIVATE
    nlohmann_json::nlohmann_json
    radbuzz_interface
    trip_compactor
    trip_recorder
)
