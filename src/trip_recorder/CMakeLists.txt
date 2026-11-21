add_library(trip_recorder EXCLUDE_FROM_ALL
    trip_recorder.cc
)

target_include_directories(trip_recorder
PUBLIC
    include
)

target_link_libraries(trip_recorder
PUBLIC
    application_state
    base_thread
    filesystem
    post_office
    radbuzz_messages
)
