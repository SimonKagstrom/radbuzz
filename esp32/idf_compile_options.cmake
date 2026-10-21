# ESP-IDF only applies its compile options (optimization level, -ffunction-sections
# etc) to IDF components, so the regular CMake libraries would otherwise be built
# without optimization. Include this after project() to apply them to all targets
# created afterwards.
#
# The warning options and the C++ standard are left to the project. ESP_PLATFORM is
# not defined, since that makes lvgl include esp_attr.h everywhere.
idf_build_get_property(idf_compile_definitions COMPILE_DEFINITIONS)
list(REMOVE_ITEM idf_compile_definitions ESP_PLATFORM)
add_compile_definitions(${idf_compile_definitions})

foreach(lang_and_property IN ITEMS ";COMPILE_OPTIONS" "C;C_COMPILE_OPTIONS" "CXX;CXX_COMPILE_OPTIONS" "ASM;ASM_COMPILE_OPTIONS")
    list(GET lang_and_property 0 lang)
    list(GET lang_and_property 1 property)
    idf_build_get_property(idf_compile_options ${property})
    list(FILTER idf_compile_options EXCLUDE REGEX "^-W|^-std=")
    foreach(option IN LISTS idf_compile_options)
        if(lang)
            add_compile_options($<$<COMPILE_LANGUAGE:${lang}>:${option}>)
        else()
            add_compile_options(${option})
        endif()
    endforeach()
endforeach()
