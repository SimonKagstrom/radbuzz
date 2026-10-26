/**
 * @file lv_conf.h
 * LVGL configuration. Only settings which differ from the LVGL defaults are
 * listed here, see lv_conf_template.h in LVGL for the full set of options.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* The ESP-IDF configuration, for platform specific settings (ESP_PLATFORM is not defined
 * for LVGL, since it's built as a regular CMake library and not as an IDF component) */
#if __has_include("sdkconfig.h")
    #include "sdkconfig.h"
#endif

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/*========================
 * RENDERING CONFIGURATION
 *========================*/

#define LV_DRAW_BUF_STRIDE_ALIGN 2
#define LV_DRAW_BUF_ALIGN 64
/* Used (and checked against the cache line size) by the PPA driver */
#define CONFIG_LV_DRAW_BUF_ALIGN LV_DRAW_BUF_ALIGN

#define LV_DRAW_SW_SUPPORT_RGB565A8     0
#define LV_DRAW_SW_SUPPORT_RGB888       0
#define LV_DRAW_SW_SUPPORT_XRGB8888     0
#define LV_DRAW_SW_SUPPORT_L8           0
#define LV_DRAW_SW_SUPPORT_AL88         0
#define LV_DRAW_SW_SUPPORT_A8           0
#define LV_DRAW_SW_SUPPORT_I1           0

/** Draw using espressif PPA accelerator */
#if CONFIG_SOC_PPA_SUPPORTED
    #define LV_USE_PPA      1
    #define LV_USE_PPA_IMG  1
#endif

/*=================
 * OTHER SETTINGS
 *=================*/

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1

#define LV_ATTRIBUTE_MEM_ALIGN __attribute__((aligned(4)))

/*==================
 *   FONT USAGE
 *===================*/

#define LV_FONT_MONTSERRAT_14 0

/* The (tiny) default font, only until the UI sets the theme with the radbuzz font */
#define LV_FONT_UNSCII_8 1
#define LV_FONT_DEFAULT &lv_font_unscii_8

/*==================
 * WIDGETS
 *================*/

#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_LED        0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0

/*==================
 * THEMES
 *==================*/

#define LV_THEME_DEFAULT_DARK 1

/*==================
 * OTHERS
 *==================*/

#define LV_USE_SYSMON 1
#define LV_SYSMON_GET_IDLE lv_timer_get_idle

/* Uncomment to show the CPU usage and FPS on the display */
// #define LV_USE_PERF_MONITOR 1
// #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT

/*==================
 * EXAMPLES
 *==================*/

#define LV_BUILD_EXAMPLES 0

#endif /*LV_CONF_H*/
