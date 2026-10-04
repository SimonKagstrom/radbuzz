add_library(radbuzz_messages INTERFACE)

target_include_directories(radbuzz_messages
INTERFACE
    include
)

target_link_libraries(radbuzz_messages
INTERFACE
    base_messages
    post_office
)