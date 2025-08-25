include (libmaelir/cmake/convert_font.cmake)

#add_subdirectory(libmaelir)
add_subdirectory(lvgl)




if ("${IDF_TARGET}" STREQUAL "")
    # Build as shared for host, since this otherwise conflicts with libz
    set(LIB_TYPE SHARED)
    message("Building pngdec_zlib as shared library, to avoid host conflicts")
else()
    set(LIB_TYPE STATIC)
    message("Building pngdec_zlib as a static library")
endif()

add_library(pngdec_zlib ${LIB_TYPE} EXCLUDE_FROM_ALL
    PNGdec/src/adler32.c
    PNGdec/src/crc32.c
    PNGdec/src/infback.c
    PNGdec/src/inffast.c
    PNGdec/src/inflate.c
    PNGdec/src/inftrees.c
    PNGdec/src/inftrees.c
    PNGdec/src/zutil.c
)

target_compile_options(pngdec_zlib
PRIVATE
    -Wno-deprecated-non-prototype
)

# Add here, since pngdec doesn't have a CMakeLists.txt
add_library(pngdec ${LIB_TYPE} EXCLUDE_FROM_ALL
    PNGdec/src/PNGdec.cpp
)
target_link_libraries(pngdec
PRIVATE
    # Link privately, to avoid conflicts with system libz
    pngdec_zlib
)

target_compile_definitions(pngdec
PUBLIC
    -D__MACH__
)

target_include_directories(pngdec PUBLIC
    PNGdec/src/
)
