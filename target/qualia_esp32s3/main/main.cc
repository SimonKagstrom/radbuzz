#include "button_debouncer.hh"
#include "gpio_esp32.hh"
#include "nvm_esp32.hh"
#include "rotary_encoder.hh"
#include "sdkconfig.h"
#include "st7701_display_esp32.hh"
#include "uart_esp32.hh"

#include <esp_io_expander_tca9554.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_io_additions.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{

constexpr auto kPinButton = GPIO_NUM_5; // SCK
constexpr auto kPinA = GPIO_NUM_6;      // MISO
constexpr auto kPinB = GPIO_NUM_7;      // MOSI


constexpr auto kTftDEPin = 2;
constexpr auto kTftVSYNCPin = 42;
constexpr auto kTftHSYNCPin = 41;
constexpr auto kTftPCLKPin = 1;
constexpr auto kTftR1Pin = 11;
constexpr auto kTftR2Pin = 10;
constexpr auto kTftR3Pin = 9;
constexpr auto kTftR4Pin = 46;
constexpr auto kTftR5Pin = 3;
constexpr auto kTftG0Pin = 48;
constexpr auto kTftG1Pin = 47;
constexpr auto kTftG2Pin = 21;
constexpr auto kTftG3Pin = 14;
constexpr auto kTftG4Pin = 13;
constexpr auto kTftG5Pin = 12;
constexpr auto kTftB1Pin = 40;
constexpr auto kTftB2Pin = 39;
constexpr auto kTftB3Pin = 38;
constexpr auto kTftB4Pin = 0;
constexpr auto kTftB5Pin = 45;

constexpr auto kI2cBus = I2C_NUM_0;
constexpr auto kI2cSdaPin = GPIO_NUM_8;
constexpr auto kI2cSclPin = GPIO_NUM_18;

constexpr auto kI2cExpanderAddress = ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_111;

constexpr auto kExpanderTftReset = IO_EXPANDER_PIN_NUM_2;
constexpr auto kExpanderTftCs = IO_EXPANDER_PIN_NUM_1;
constexpr auto kExpanderTftBacklight = IO_EXPANDER_PIN_NUM_4;
constexpr auto kExpanderTftSck = IO_EXPANDER_PIN_NUM_0;
constexpr auto kExpanderTftMosi = IO_EXPANDER_PIN_NUM_7;


std::unique_ptr<DisplayTarget>
CreateDisplay()
{
    i2c_master_bus_handle_t bus_handle = nullptr;
    esp_io_expander_handle_t expander_handle = nullptr;

    const i2c_master_bus_config_t i2c_master_config = {
        .i2c_port = kI2cBus,
        .sda_io_num = kI2cSdaPin,
        .scl_io_num = kI2cSclPin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags {.enable_internal_pullup = true, .allow_pd = false}};
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_config, &bus_handle));

    ESP_ERROR_CHECK(
        esp_io_expander_new_i2c_tca9554(bus_handle, kI2cExpanderAddress, &expander_handle));

    esp_io_expander_set_dir(expander_handle, kExpanderTftReset, IO_EXPANDER_OUTPUT);

    esp_io_expander_set_dir(expander_handle, kExpanderTftCs, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_dir(expander_handle, kExpanderTftSck, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_dir(expander_handle, kExpanderTftMosi, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_dir(expander_handle, kExpanderTftBacklight, IO_EXPANDER_OUTPUT);

    esp_io_expander_set_level(
        expander_handle, kExpanderTftReset | kExpanderTftMosi | kExpanderTftCs, 1);

    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_gpio_num = kExpanderTftCs,
        .scl_io_type = IO_TYPE_EXPANDER,
        .scl_gpio_num = kExpanderTftSck,
        .sda_io_type = IO_TYPE_EXPANDER,
        .sda_gpio_num = kExpanderTftMosi,
        .io_expander = expander_handle,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = {
        .line_config = line_config,
        .expect_clk_speed = 500 * 1000U,
        .spi_mode = 0,
        .lcd_cmd_bytes = 1,
        .lcd_param_bytes = 1,
        .flags =
            {
                .use_dc_bit = 1,
                .dc_zero_on_data = 0,
                .lsb_first = 0,
                .cs_high_active = 0,
                .del_keep_cs_inactive = 1,
            },
    };


    const esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_PLL240M,
        .timings =
            {
                .pclk_hz = 15 * 1000 * 1000,
                .h_res = hal::kDisplayWidth,
                .v_res = hal::kDisplayHeight,
                .hsync_pulse_width = 8,
                .hsync_back_porch = 10,
                .hsync_front_porch = 50,
                .vsync_pulse_width = 2,
                .vsync_back_porch = 18,
                .vsync_front_porch = 8,
                .flags {
                    .hsync_idle_low = false,
                    .vsync_idle_low = false,
                    .de_idle_high = false,
                    .pclk_active_neg = false,
                    .pclk_idle_high = false,
                },
            },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 0,
        .bounce_buffer_size_px = hal::kDisplayWidth * 10,
        .sram_trans_align = 64, // Deprecated
        .dma_burst_size = 64,
        .hsync_gpio_num = kTftHSYNCPin,
        .vsync_gpio_num = kTftVSYNCPin,
        .de_gpio_num = kTftDEPin,
        .pclk_gpio_num = kTftPCLKPin,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums =
            {
                kTftB1Pin,
                kTftB2Pin,
                kTftB3Pin,
                kTftB4Pin,
                kTftB5Pin,
                kTftG0Pin,
                kTftG1Pin,
                kTftG2Pin,
                kTftG3Pin,
                kTftG4Pin,
                kTftG5Pin,
                kTftR1Pin,
                kTftR2Pin,
                kTftR3Pin,
                kTftR4Pin,
                kTftR5Pin,
            },
        .flags {
            .disp_active_low = 0,
            .refresh_on_demand = 0,
            .fb_in_psram = 0,
            .double_fb = 0,
            .no_fb = 1,
            .bb_invalidate_cache = 0,
        },
    };

    auto out = std::make_unique<DisplayTarget>(io_config, rgb_config);

    // Turn on the backlight
    esp_io_expander_set_level(expander_handle, kExpanderTftBacklight, 1);
    // For now, maybe keep it in the future
    esp_io_expander_del(expander_handle);
    i2c_del_master_bus(bus_handle);

    return out;
}

} // namespace

extern "C" void
app_main(void)
{
    gpio_config_t io_conf = {};

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1 << kPinA) | (1 << kPinB) | (1 << kPinButton);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));


    // Install the GPIO interrupt service
    gpio_install_isr_service(0);

    auto target_nvm = std::make_unique<NvmTarget>();

    auto button_debouncer = std::make_unique<ButtonDebouncer>();

    auto pin_a_gpio = std::make_unique<TargetGpio>(kPinA);
    auto pin_b_gpio = std::make_unique<TargetGpio>(kPinB);
    auto button_gpio = button_debouncer->AddButton(
        std::make_unique<TargetGpio>(kPinButton, TargetGpio::Polarity::kActiveLow));

    auto rotary_encoder = std::make_unique<RotaryEncoder>(*pin_a_gpio, *pin_b_gpio);

    auto display = CreateDisplay();

    while (true)
    {
        vTaskSuspend(nullptr);
    }
}
