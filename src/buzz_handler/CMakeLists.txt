add_library(buzz_handler EXCLUDE_FROM_ALL
    buzz_handler.cc
)

target_include_directories(buzz_handler
PUBLIC
    include
)

target_link_libraries(buzz_handler
PUBLIC
    base_thread
    application_state
)
