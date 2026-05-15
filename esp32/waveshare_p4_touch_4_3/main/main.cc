#include "app_simulator.hh"
#include "ble_handler.hh"
#include "ble_server_esp32.hh"
#include "blitter_esp32.hh"
#include "button_debouncer.hh"
#include "buzz_handler.hh"
#include "can_bus_handler.hh"
#include "can_esp32.hh"
#include "filesystem.hh"
#include "gpio_esp32.hh"
#include "gps_reader.hh"
#include "i2c_gps_esp32.hh"
#include "image_cache.hh"
#include "input.hh"
#include "nvm_esp32.hh"
#include "pm_esp32.hh"
#include "rotary_encoder.hh"
#include "sdkconfig.h"
#include "speedometer_handler.hh"
#include "st7701_display_esp32.hh"
#include "stepper_motor_esp32.hh"
#include "storage.hh"
#include "touch_esp32.hh"
#include "trip_computer.hh"
#include "uart_esp32.hh"
#include "uart_gps_esp32.hh"
#include "user_interface.hh"
#include "wifi_client_esp32.hh"
#include "wifi_handler.hh"

#include <driver/ledc.h>
#include <driver/sdmmc_host.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_st7701.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_ldo_regulator.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>
#include <sstream>

namespace
{

constexpr auto kTftBacklight = GPIO_NUM_26;

constexpr auto kPinLeftBuzzer = GPIO_NUM_25;  // TODO
constexpr auto kPinRightBuzzer = GPIO_NUM_49; // TODO

constexpr auto kPinStepperSleepGpio = GPIO_NUM_30;
constexpr auto kPinStepperDirGpio = GPIO_NUM_46;
constexpr auto kPinStepGpio = GPIO_NUM_31;

constexpr auto kCanBusTxPin = GPIO_NUM_22;
constexpr auto kCanBusRxPin = GPIO_NUM_29;

constexpr auto kI2cSdaPin = GPIO_NUM_7;
constexpr auto kI2cSclPin = GPIO_NUM_8;

constexpr auto kButtonGpio = GPIO_NUM_48;
constexpr auto kRotaryEncoderPinA = GPIO_NUM_52;
constexpr auto kRotaryEncoderPinB = GPIO_NUM_47;

constexpr auto kSdPwrCtrlLdoIoId = 4;

constexpr auto kLcdResetPin = GPIO_NUM_27;
constexpr auto kBacklightPin = GPIO_NUM_26; // set to -1 if not used
constexpr auto kMipiDsiLaneNum = 4;

constexpr auto kMipiDpiPxFormat = LCD_COLOR_FMT_RGB565;

constexpr auto kMipiDsiPhyPwrLdoChan = 3;
constexpr auto kMipiDsiPhyPwrLdoVoltageMv = 2500;


#define BSP_LCD_BACKLIGHT     (GPIO_NUM_26)
#define BSP_LCD_RST           (GPIO_NUM_27)
#define BSP_LCD_TOUCH_RST     (GPIO_NUM_23)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_NC)
#define BSP_LCD_PIXEL_CLOCK_MHZ     (80)

/* LCD display color format */
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB888)
#else
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
#endif
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN           (0)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE         (ESP_LCD_COLOR_SPACE_RGB)

#define BSP_LCD_H_RES              (480)
#define BSP_LCD_V_RES              (800)
#define BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS (1000)

#define BSP_LCD_MIPI_DSI_LANE_NUM          (2)    // 2 data lanes
#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

// Bit number used to represent command and parameter
#define LCD_LEDC_CH 1


constexpr st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFF, (uint8_t[]) {0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]) {0x08}, 1, 0},
    {0xFF, (uint8_t[]) {0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]) {0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]) {0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]) {0x17, 0x08}, 2, 0},
    {0xCC, (uint8_t[]) {0x10}, 1, 0},
    {0xB0,
     (uint8_t[]) {0x40,
                  0xC9,
                  0x94,
                  0x0E,
                  0x10,
                  0x05,
                  0x0B,
                  0x09,
                  0x08,
                  0x26,
                  0x04,
                  0x52,
                  0x10,
                  0x69,
                  0x6B,
                  0x69},
     16,
     0},

    {0xB1,
     (uint8_t[]) {0x40,
                  0xD2,
                  0x98,
                  0x0C,
                  0x92,
                  0x07,
                  0x09,
                  0x08,
                  0x07,
                  0x25,
                  0x02,
                  0x0E,
                  0x0C,
                  0x6E,
                  0x78,
                  0x55},
     16,
     0},

    {0xFF, (uint8_t[]) {0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]) {0x5D}, 1, 0},
    {0xB1, (uint8_t[]) {0x4E}, 1, 0},
    {0xB2, (uint8_t[]) {0x87}, 1, 0},
    {0xB3, (uint8_t[]) {0x80}, 1, 0},
    {0xB5, (uint8_t[]) {0x4E}, 1, 0},
    {0xB7, (uint8_t[]) {0x85}, 1, 0},
    {0xB8, (uint8_t[]) {0x21}, 1, 0},
    {0xB9, (uint8_t[]) {0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]) {0x03}, 1, 0},
    {0xBC, (uint8_t[]) {0x00}, 1, 0},

    {0xC1, (uint8_t[]) {0x78}, 1, 0},
    {0xC2, (uint8_t[]) {0x78}, 1, 0},
    {0xD0, (uint8_t[]) {0x88}, 1, 0},
    {0xE0, (uint8_t[]) {0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]) {0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},

    {0xE2,
     (uint8_t[]) {0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00},
     13,
     0},

    {0xE3, (uint8_t[]) {0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]) {0x44, 0x44}, 2, 0},
    {0xE5,
     (uint8_t[]) {0x09,
                  0x2E,
                  0xA0,
                  0xA0,
                  0x0B,
                  0x30,
                  0xA0,
                  0xA0,
                  0x05,
                  0x2A,
                  0xA0,
                  0xA0,
                  0x07,
                  0x2C,
                  0xA0,
                  0xA0},
     16,
     0},

    {0xE6, (uint8_t[]) {0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]) {0x44, 0x44}, 2, 0},
    {0xE8,
     (uint8_t[]) {0x08,
                  0x2D,
                  0xA0,
                  0xA0,
                  0x0A,
                  0x2F,
                  0xA0,
                  0xA0,
                  0x04,
                  0x29,
                  0xA0,
                  0xA0,
                  0x06,
                  0x2B,
                  0xA0,
                  0xA0},
     16,
     0},

    {0xEB, (uint8_t[]) {0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]) {0x08, 0x01}, 2, 0},

    {0xED,
     (uint8_t[]) {0xB0,
                  0x2B,
                  0x98,
                  0xA4,
                  0x56,
                  0x7F,
                  0xFF,
                  0xFF,
                  0xFF,
                  0xFF,
                  0xF7,
                  0x65,
                  0x4A,
                  0x89,
                  0xB2,
                  0x0B},
     16,
     0},

    {0xEF, (uint8_t[]) {0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]) {0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]) {0x00}, 0, 120},
    {0x29, (uint8_t[]) {0x00}, 0, 0},
};


auto
CreateDisplay()
{
    static esp_ldo_channel_handle_t ldo_mipi_phy = nullptr;
    static esp_lcd_dsi_bus_handle_t mipi_dsi_bus = nullptr;
    static esp_lcd_panel_io_handle_t mipi_dbi_io = nullptr;

    esp_ldo_channel_config_t ldo_mipi_phy_config {};
    ldo_mipi_phy_config.chan_id = kMipiDsiPhyPwrLdoChan;
    ldo_mipi_phy_config.voltage_mv = kMipiDsiPhyPwrLdoVoltageMv;
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

    static esp_lcd_dsi_bus_config_t bus_config {};
    bus_config.bus_id = 0;
    bus_config.num_data_lanes = BSP_LCD_MIPI_DSI_LANE_NUM;
    // Below esp32p4 v3
    bus_config.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY;
    bus_config.lane_bit_rate_mbps = 1000;
    bus_config.flags.clock_lane_force_hs = false;

    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    static esp_lcd_dbi_io_config_t dbi_config {};
    dbi_config.virtual_channel = 0;
    dbi_config.lcd_cmd_bits = 8;
    dbi_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));
    static esp_lcd_dpi_panel_config_t dpi_config {};
    dpi_config.virtual_channel = 0;
    dpi_config.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi_config.dpi_clock_freq_mhz = 36;
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.out_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.num_fbs = 2;
    dpi_config.video_timing.h_size = BSP_LCD_H_RES;
    dpi_config.video_timing.v_size = BSP_LCD_V_RES;
    dpi_config.video_timing.hsync_pulse_width = 12;
    dpi_config.video_timing.hsync_back_porch = 42;
    dpi_config.video_timing.hsync_front_porch = 42;
    dpi_config.video_timing.vsync_pulse_width = 8;
    dpi_config.video_timing.vsync_back_porch = 2;
    dpi_config.video_timing.vsync_front_porch = 60;
    dpi_config.flags.disable_lp = false;

    static st7701_vendor_config_t vendor_config {};
    vendor_config.init_cmds = lcd_init_cmds;
    vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);
    vendor_config.mipi_config.dsi_bus = mipi_dsi_bus;
    vendor_config.mipi_config.dpi_config = &dpi_config;
    vendor_config.flags.use_mipi_interface = 1;

    static esp_lcd_panel_dev_config_t panel_config {};
    panel_config.reset_gpio_num = BSP_LCD_RST;
    panel_config.bits_per_pixel = 16;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
    panel_config.vendor_config = &vendor_config;

    auto out = std::make_unique<DisplaySt7701>(mipi_dbi_io, panel_config);

    return out;
}

} // namespace

extern "C" void
app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ApplicationState application_state;

    gpio_config_t io_conf = {};
//
//    io_conf.mode = GPIO_MODE_OUTPUT;
//    io_conf.pin_bit_mask = (1ull << kTftBacklight) | (1ull << kPinLeftBuzzer) |
//                           (1ull << kPinRightBuzzer) | (1ull << kPinStepperSleepGpio) |
//                           (1ull << kPinStepperDirGpio);
//    gpio_config(&io_conf);

    io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask =
        (1ull << kButtonGpio) | (1ull << kRotaryEncoderPinA) | (1ull << kRotaryEncoderPinB);
    gpio_config(&io_conf);


    // Turn on the backlight
    gpio_set_level(kTftBacklight, 0);


    auto display = CreateDisplay();

    // Create before SD card (see below)
    auto wifi_client = std::make_unique<WifiClientEsp32>();

    sdmmc_host_t sd_mmc_host_config = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    sd_mmc_host_config.slot = SDMMC_HOST_SLOT_0;
    // See https://github.com/espressif/esp-hosted-mcu/blob/main/examples/host_sdcard_with_hosted
    sd_mmc_host_config.init = []() { return ESP_OK; };
    sd_mmc_host_config.deinit = []() { return ESP_OK; };
    sd_mmc_host_config.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = kSdPwrCtrlLdoIoId,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    auto ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        printf("Failed to create a new on-chip LDO power control driver");
        return;
    }
    sd_mmc_host_config.pwr_ctrl_handle = pwr_ctrl_handle;

    // The filesystem
    sdmmc_host_init();

    slot_config.width = 4;
    slot_config.clk = GPIO_NUM_43;
    slot_config.cmd = GPIO_NUM_44;
    slot_config.d0 = GPIO_NUM_39;
    slot_config.d1 = GPIO_NUM_40;
    slot_config.d2 = GPIO_NUM_41;
    slot_config.d3 = GPIO_NUM_42;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = true,
        .use_one_fat = false,
    };
    sdmmc_card_t* card;

    auto err =
        esp_vfs_fat_sdmmc_mount("/sdcard", &sd_mmc_host_config, &slot_config, &mount_config, &card);
    if (err == ESP_OK)
    {
        sdmmc_card_print_info(stdout, card);
    }

    const i2c_master_bus_config_t i2c_mst_config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = static_cast<gpio_num_t>(kI2cSdaPin),
        .scl_io_num = static_cast<gpio_num_t>(kI2cSclPin),
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = true,
                .allow_pd = false,
            },
    };

    i2c_master_bus_handle_t i2c_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_handle));
    esp_lcd_panel_io_i2c_config_t io_config = {.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
                                               .scl_speed_hz = 100000,
                                               .control_phase_bytes = 1,
                                               .dc_bit_offset = 0,
                                               .lcd_cmd_bits = 16,
                                               .lcd_param_bits = 0,
                                               .on_color_trans_done = nullptr,
                                               .user_ctx = nullptr,
                                               .flags = {
                                                   .dc_low_on_data = 0,
                                                   .disable_control_phase = 1,
                                               }};


    esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
        .dev_addr = static_cast<uint8_t>(io_config.dev_addr),
    };

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = hal::kDisplayWidth,
        .y_max = hal::kDisplayHeight,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels =
            {
                .reset = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = 1,
                .mirror_x = 0,
                .mirror_y = 1,
            },
        .process_coordinates = nullptr,
        .interrupt_callback = nullptr,
        .user_data = nullptr,
        .driver_data = &tp_gt911_config,
    };
    esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
    esp_lcd_touch_handle_t tp;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &io_config, &tp_io_handle));
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));

    // Devices / helper classes
    //auto left_buzzer_gpio = std::make_unique<TargetGpio>(kPinLeftBuzzer);
    //auto right_buzzer_gpio = std::make_unique<TargetGpio>(kPinRightBuzzer);
    auto image_cache = std::make_unique<ImageCache>();
    //    auto uart1 = std::make_unique<TargetUart>(UART_NUM_1,
    //                                              9600,
    //                                              GPIO_NUM_44,  // RX
    //                                              GPIO_NUM_43); // TX
    //

    //auto gps = std::make_unique<I2cGps>(kI2cSclPin, kI2cSdaPin);
    //auto uart_gps = std::make_unique<UartGps>(*uart1);
    auto filesystem = std::make_unique<Filesystem>("/sdcard/app_data/");

    auto httpd_client = std::make_unique<HttpdClient>();

    auto blitter = std::make_unique<BlitterEsp32>();
    auto pm = std::make_unique<PmEsp32>();
    auto can = std::make_unique<CanEsp32>(kCanBusTxPin, kCanBusRxPin, 500000);
    //auto stepper_sleep_gpio =
    //    std::make_unique<TargetGpio>(kPinStepperSleepGpio, TargetGpio::Polarity::kActiveHigh);
    //auto stepper_dir_gpio =
    //    std::make_unique<TargetGpio>(kPinStepperDirGpio, TargetGpio::Polarity::kActiveLow);
    auto pin_a = std::make_unique<TargetGpio>(kRotaryEncoderPinA);
    auto pin_b = std::make_unique<TargetGpio>(kRotaryEncoderPinB);

//    auto stepper_motor =
//        std::make_unique<StepperMotorEsp32>(*stepper_sleep_gpio, *stepper_dir_gpio, kPinStepGpio);
//
    auto nvm = std::make_unique<NvmTarget>();

//    stepper_motor->Start();

    auto touch = std::make_unique<TouchEsp32>(tp);
    auto rotary_encoder = std::make_unique<RotaryEncoder>(*pin_a, *pin_b);
    auto button_debouncer = std::make_unique<ButtonDebouncer>();
    auto debounced_button = button_debouncer->AddButton(
        std::make_unique<TargetGpio>(kButtonGpio, TargetGpio::Polarity::kActiveLow));

    auto input = std::make_unique<Input>(*debounced_button, *rotary_encoder, *touch);

    // Threads
    auto wifi_handler = std::make_unique<WifiHandler>(application_state, *filesystem, *wifi_client);
    auto storage = std::make_unique<Storage>(application_state, *nvm);
  //  auto buzz_handler =
  //      std::make_unique<BuzzHandler>(*left_buzzer_gpio, *right_buzzer_gpio, application_state);
    //auto ble_server = std::make_unique<BleServerEsp32>();
    auto ble_server = std::make_unique<BleServerHost>();
    auto app_simulator = std::make_unique<AppSimulator>(application_state, *ble_server);
    auto can_bus_handler = std::make_unique<CanBusHandler>(*can, application_state, 0x6f);

    //auto gps_reader = std::make_unique<GpsReader>(application_state, *gps);
    auto tile_cache = std::make_unique<TileCache>(
        application_state, pm->CreateFullPowerLock(), *filesystem, *httpd_client);
    auto ble_handler = std::make_unique<BleHandler>(*ble_server, application_state, *image_cache);

    auto trip_computer = std::make_unique<TripComputer>(application_state);

//    constexpr auto kFullRotation = 2400;
//    auto speedometer_handler =
//        std::make_unique<SpeedometerHandler>(*stepper_motor, application_state, kFullRotation);

    auto user_interface = std::make_unique<UserInterface>(*display,
                                                          *blitter,
                                                          pm->CreateFullPowerLock(),
                                                          *input,
                                                          application_state,
                                                          *image_cache,
                                                          *tile_cache);


    storage->Start("storage");
    input->Start("input");
    button_debouncer->Start("button_debouncer", os::ThreadPriority::kHigh);
  //  buzz_handler->Start("buzz_handler", 8192);
    app_simulator->Start("app_simulator", 8192);
    can_bus_handler->Start("can_bus_handler", 4096);
    ble_handler->Start("ble_server", 8192);
    wifi_handler->Start("wifi_handler", 8192);
    //speedometer_handler->Start("speedometer_handler");
    trip_computer->Start("trip_computer");

    //gps_reader->Start("gps_reader");
    tile_cache->Start("tile_cache", 8192);
    user_interface->Start("user_interface", os::ThreadCore::kCore1, 8192);

    // TMP!
    os::Sleep(2s);
    //ble_server->ScanForService(hal::Uuid16(0x61c9), [](auto peer) { printf("Found peer\n"); });

    while (true)
    {
        vTaskSuspend(nullptr);
    }
}
