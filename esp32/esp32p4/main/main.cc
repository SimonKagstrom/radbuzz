#include "app_simulator.hh"
#include "ble_handler.hh"
#include "ble_server_esp32.hh"
#include "button_debouncer.hh"
#include "buzz_handler.hh"
#include "can_bus_handler.hh"
#include "can_esp32.hh"
#include "filesystem.hh"
#include "gpio_esp32.hh"
#include "i2c_gps_esp32.hh"
#include "image_cache.hh"
#include "jd9365_display_esp32.hh"
#include "nvm_esp32.hh"
#include "pm_esp32.hh"
#include "rotary_encoder.hh"
#include "sdkconfig.h"
#include "uart_esp32.hh"
#include "uart_gps_esp32.hh"
#include "user_interface.hh"
#include "wifi_client_esp32.hh"

#include <driver/sdmmc_host.h>
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
constexpr auto kPinLeftBuzzer = GPIO_NUM_32;  // TODO
constexpr auto kPinRightBuzzer = GPIO_NUM_48; // TODO
constexpr auto kI2cSdaPin = GPIO_NUM_7;
constexpr auto kI2cSclPin = GPIO_NUM_8;


#define TEST_LCD_BIT_PER_PIXEL (16)
#define TEST_PIN_NUM_LCD_RST   (27)
#define TEST_PIN_NUM_BK_LIGHT  (26) // set to -1 if not used
#define TEST_MIPI_DSI_LANE_NUM (4)

#define TEST_MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB565)

#define TEST_DELAY_TIME_MS (3000)

#define TEST_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)
#define TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

constexpr jd9365_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xE0, (uint8_t[]) {0x00}, 1, 0},

    {0xE1, (uint8_t[]) {0x93}, 1, 0},   {0xE2, (uint8_t[]) {0x65}, 1, 0},
    {0xE3, (uint8_t[]) {0xF8}, 1, 0},   {0x80, (uint8_t[]) {0x01}, 1, 0},

    {0xE0, (uint8_t[]) {0x01}, 1, 0},

    {0x00, (uint8_t[]) {0x00}, 1, 0},   {0x01, (uint8_t[]) {0x41}, 1, 0},
    {0x03, (uint8_t[]) {0x10}, 1, 0},   {0x04, (uint8_t[]) {0x44}, 1, 0},

    {0x17, (uint8_t[]) {0x00}, 1, 0},   {0x18, (uint8_t[]) {0xD0}, 1, 0},
    {0x19, (uint8_t[]) {0x00}, 1, 0},   {0x1A, (uint8_t[]) {0x00}, 1, 0},
    {0x1B, (uint8_t[]) {0xD0}, 1, 0},   {0x1C, (uint8_t[]) {0x00}, 1, 0},

    {0x24, (uint8_t[]) {0xFE}, 1, 0},   {0x35, (uint8_t[]) {0x26}, 1, 0},

    {0x37, (uint8_t[]) {0x09}, 1, 0},

    {0x38, (uint8_t[]) {0x04}, 1, 0},   {0x39, (uint8_t[]) {0x08}, 1, 0},
    {0x3A, (uint8_t[]) {0x0A}, 1, 0},   {0x3C, (uint8_t[]) {0x78}, 1, 0},
    {0x3D, (uint8_t[]) {0xFF}, 1, 0},   {0x3E, (uint8_t[]) {0xFF}, 1, 0},
    {0x3F, (uint8_t[]) {0xFF}, 1, 0},

    {0x40, (uint8_t[]) {0x00}, 1, 0},   {0x41, (uint8_t[]) {0x64}, 1, 0},
    {0x42, (uint8_t[]) {0xC7}, 1, 0},   {0x43, (uint8_t[]) {0x18}, 1, 0},
    {0x44, (uint8_t[]) {0x0B}, 1, 0},   {0x45, (uint8_t[]) {0x14}, 1, 0},

    {0x55, (uint8_t[]) {0x02}, 1, 0},   {0x57, (uint8_t[]) {0x49}, 1, 0},
    {0x59, (uint8_t[]) {0x0A}, 1, 0},   {0x5A, (uint8_t[]) {0x1B}, 1, 0},
    {0x5B, (uint8_t[]) {0x19}, 1, 0},

    {0x5D, (uint8_t[]) {0x7F}, 1, 0},   {0x5E, (uint8_t[]) {0x56}, 1, 0},
    {0x5F, (uint8_t[]) {0x43}, 1, 0},   {0x60, (uint8_t[]) {0x37}, 1, 0},
    {0x61, (uint8_t[]) {0x33}, 1, 0},   {0x62, (uint8_t[]) {0x25}, 1, 0},
    {0x63, (uint8_t[]) {0x2A}, 1, 0},   {0x64, (uint8_t[]) {0x16}, 1, 0},
    {0x65, (uint8_t[]) {0x30}, 1, 0},   {0x66, (uint8_t[]) {0x2F}, 1, 0},
    {0x67, (uint8_t[]) {0x32}, 1, 0},   {0x68, (uint8_t[]) {0x53}, 1, 0},
    {0x69, (uint8_t[]) {0x43}, 1, 0},   {0x6A, (uint8_t[]) {0x4C}, 1, 0},
    {0x6B, (uint8_t[]) {0x40}, 1, 0},   {0x6C, (uint8_t[]) {0x3D}, 1, 0},
    {0x6D, (uint8_t[]) {0x31}, 1, 0},   {0x6E, (uint8_t[]) {0x20}, 1, 0},
    {0x6F, (uint8_t[]) {0x0F}, 1, 0},

    {0x70, (uint8_t[]) {0x7F}, 1, 0},   {0x71, (uint8_t[]) {0x56}, 1, 0},
    {0x72, (uint8_t[]) {0x43}, 1, 0},   {0x73, (uint8_t[]) {0x37}, 1, 0},
    {0x74, (uint8_t[]) {0x33}, 1, 0},   {0x75, (uint8_t[]) {0x25}, 1, 0},
    {0x76, (uint8_t[]) {0x2A}, 1, 0},   {0x77, (uint8_t[]) {0x16}, 1, 0},
    {0x78, (uint8_t[]) {0x30}, 1, 0},   {0x79, (uint8_t[]) {0x2F}, 1, 0},
    {0x7A, (uint8_t[]) {0x32}, 1, 0},   {0x7B, (uint8_t[]) {0x53}, 1, 0},
    {0x7C, (uint8_t[]) {0x43}, 1, 0},   {0x7D, (uint8_t[]) {0x4C}, 1, 0},
    {0x7E, (uint8_t[]) {0x40}, 1, 0},   {0x7F, (uint8_t[]) {0x3D}, 1, 0},
    {0x80, (uint8_t[]) {0x31}, 1, 0},   {0x81, (uint8_t[]) {0x20}, 1, 0},
    {0x82, (uint8_t[]) {0x0F}, 1, 0},

    {0xE0, (uint8_t[]) {0x02}, 1, 0},   {0x00, (uint8_t[]) {0x5F}, 1, 0},
    {0x01, (uint8_t[]) {0x5F}, 1, 0},   {0x02, (uint8_t[]) {0x5E}, 1, 0},
    {0x03, (uint8_t[]) {0x5E}, 1, 0},   {0x04, (uint8_t[]) {0x50}, 1, 0},
    {0x05, (uint8_t[]) {0x48}, 1, 0},   {0x06, (uint8_t[]) {0x48}, 1, 0},
    {0x07, (uint8_t[]) {0x4A}, 1, 0},   {0x08, (uint8_t[]) {0x4A}, 1, 0},
    {0x09, (uint8_t[]) {0x44}, 1, 0},   {0x0A, (uint8_t[]) {0x44}, 1, 0},
    {0x0B, (uint8_t[]) {0x46}, 1, 0},   {0x0C, (uint8_t[]) {0x46}, 1, 0},
    {0x0D, (uint8_t[]) {0x5F}, 1, 0},   {0x0E, (uint8_t[]) {0x5F}, 1, 0},
    {0x0F, (uint8_t[]) {0x57}, 1, 0},   {0x10, (uint8_t[]) {0x57}, 1, 0},
    {0x11, (uint8_t[]) {0x77}, 1, 0},   {0x12, (uint8_t[]) {0x77}, 1, 0},
    {0x13, (uint8_t[]) {0x40}, 1, 0},   {0x14, (uint8_t[]) {0x42}, 1, 0},
    {0x15, (uint8_t[]) {0x5F}, 1, 0},

    {0x16, (uint8_t[]) {0x5F}, 1, 0},   {0x17, (uint8_t[]) {0x5F}, 1, 0},
    {0x18, (uint8_t[]) {0x5E}, 1, 0},   {0x19, (uint8_t[]) {0x5E}, 1, 0},
    {0x1A, (uint8_t[]) {0x50}, 1, 0},   {0x1B, (uint8_t[]) {0x49}, 1, 0},
    {0x1C, (uint8_t[]) {0x49}, 1, 0},   {0x1D, (uint8_t[]) {0x4B}, 1, 0},
    {0x1E, (uint8_t[]) {0x4B}, 1, 0},   {0x1F, (uint8_t[]) {0x45}, 1, 0},
    {0x20, (uint8_t[]) {0x45}, 1, 0},   {0x21, (uint8_t[]) {0x47}, 1, 0},
    {0x22, (uint8_t[]) {0x47}, 1, 0},   {0x23, (uint8_t[]) {0x5F}, 1, 0},
    {0x24, (uint8_t[]) {0x5F}, 1, 0},   {0x25, (uint8_t[]) {0x57}, 1, 0},
    {0x26, (uint8_t[]) {0x57}, 1, 0},   {0x27, (uint8_t[]) {0x77}, 1, 0},
    {0x28, (uint8_t[]) {0x77}, 1, 0},   {0x29, (uint8_t[]) {0x41}, 1, 0},
    {0x2A, (uint8_t[]) {0x43}, 1, 0},   {0x2B, (uint8_t[]) {0x5F}, 1, 0},

    {0x2C, (uint8_t[]) {0x1E}, 1, 0},   {0x2D, (uint8_t[]) {0x1E}, 1, 0},
    {0x2E, (uint8_t[]) {0x1F}, 1, 0},   {0x2F, (uint8_t[]) {0x1F}, 1, 0},
    {0x30, (uint8_t[]) {0x10}, 1, 0},   {0x31, (uint8_t[]) {0x07}, 1, 0},
    {0x32, (uint8_t[]) {0x07}, 1, 0},   {0x33, (uint8_t[]) {0x05}, 1, 0},
    {0x34, (uint8_t[]) {0x05}, 1, 0},   {0x35, (uint8_t[]) {0x0B}, 1, 0},
    {0x36, (uint8_t[]) {0x0B}, 1, 0},   {0x37, (uint8_t[]) {0x09}, 1, 0},
    {0x38, (uint8_t[]) {0x09}, 1, 0},   {0x39, (uint8_t[]) {0x1F}, 1, 0},
    {0x3A, (uint8_t[]) {0x1F}, 1, 0},   {0x3B, (uint8_t[]) {0x17}, 1, 0},
    {0x3C, (uint8_t[]) {0x17}, 1, 0},   {0x3D, (uint8_t[]) {0x17}, 1, 0},
    {0x3E, (uint8_t[]) {0x17}, 1, 0},   {0x3F, (uint8_t[]) {0x03}, 1, 0},
    {0x40, (uint8_t[]) {0x01}, 1, 0},   {0x41, (uint8_t[]) {0x1F}, 1, 0},

    {0x42, (uint8_t[]) {0x1E}, 1, 0},   {0x43, (uint8_t[]) {0x1E}, 1, 0},
    {0x44, (uint8_t[]) {0x1F}, 1, 0},   {0x45, (uint8_t[]) {0x1F}, 1, 0},
    {0x46, (uint8_t[]) {0x10}, 1, 0},   {0x47, (uint8_t[]) {0x06}, 1, 0},
    {0x48, (uint8_t[]) {0x06}, 1, 0},   {0x49, (uint8_t[]) {0x04}, 1, 0},
    {0x4A, (uint8_t[]) {0x04}, 1, 0},   {0x4B, (uint8_t[]) {0x0A}, 1, 0},
    {0x4C, (uint8_t[]) {0x0A}, 1, 0},   {0x4D, (uint8_t[]) {0x08}, 1, 0},
    {0x4E, (uint8_t[]) {0x08}, 1, 0},   {0x4F, (uint8_t[]) {0x1F}, 1, 0},
    {0x50, (uint8_t[]) {0x1F}, 1, 0},   {0x51, (uint8_t[]) {0x17}, 1, 0},
    {0x52, (uint8_t[]) {0x17}, 1, 0},   {0x53, (uint8_t[]) {0x17}, 1, 0},
    {0x54, (uint8_t[]) {0x17}, 1, 0},   {0x55, (uint8_t[]) {0x02}, 1, 0},
    {0x56, (uint8_t[]) {0x00}, 1, 0},   {0x57, (uint8_t[]) {0x1F}, 1, 0},

    {0xE0, (uint8_t[]) {0x02}, 1, 0},   {0x58, (uint8_t[]) {0x40}, 1, 0},
    {0x59, (uint8_t[]) {0x00}, 1, 0},   {0x5A, (uint8_t[]) {0x00}, 1, 0},
    {0x5B, (uint8_t[]) {0x30}, 1, 0},   {0x5C, (uint8_t[]) {0x01}, 1, 0},
    {0x5D, (uint8_t[]) {0x30}, 1, 0},   {0x5E, (uint8_t[]) {0x01}, 1, 0},
    {0x5F, (uint8_t[]) {0x02}, 1, 0},   {0x60, (uint8_t[]) {0x30}, 1, 0},
    {0x61, (uint8_t[]) {0x03}, 1, 0},   {0x62, (uint8_t[]) {0x04}, 1, 0},
    {0x63, (uint8_t[]) {0x04}, 1, 0},   {0x64, (uint8_t[]) {0xA6}, 1, 0},
    {0x65, (uint8_t[]) {0x43}, 1, 0},   {0x66, (uint8_t[]) {0x30}, 1, 0},
    {0x67, (uint8_t[]) {0x73}, 1, 0},   {0x68, (uint8_t[]) {0x05}, 1, 0},
    {0x69, (uint8_t[]) {0x04}, 1, 0},   {0x6A, (uint8_t[]) {0x7F}, 1, 0},
    {0x6B, (uint8_t[]) {0x08}, 1, 0},   {0x6C, (uint8_t[]) {0x00}, 1, 0},
    {0x6D, (uint8_t[]) {0x04}, 1, 0},   {0x6E, (uint8_t[]) {0x04}, 1, 0},
    {0x6F, (uint8_t[]) {0x88}, 1, 0},

    {0x75, (uint8_t[]) {0xD9}, 1, 0},   {0x76, (uint8_t[]) {0x00}, 1, 0},
    {0x77, (uint8_t[]) {0x33}, 1, 0},   {0x78, (uint8_t[]) {0x43}, 1, 0},

    {0xE0, (uint8_t[]) {0x00}, 1, 0},

    {0x11, (uint8_t[]) {0x00}, 1, 120},

    {0x29, (uint8_t[]) {0x00}, 1, 20},  {0x35, (uint8_t[]) {0x00}, 1, 0},
};


std::unique_ptr<DisplayJd9365>
CreateDisplay()
{
    static esp_ldo_channel_handle_t ldo_mipi_phy = nullptr;
    static esp_lcd_dsi_bus_handle_t mipi_dsi_bus = nullptr;
    static esp_lcd_panel_io_handle_t mipi_dbi_io = nullptr;

    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = TEST_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        .flags = {},
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

    static const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        // Below esp32p4 v3
        .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY,
        .lane_bit_rate_mbps = 1500,
    };

    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    static const esp_lcd_dbi_io_config_t dbi_config = JD9365_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));
    static const esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 40,
        .pixel_format = TEST_MIPI_DPI_PX_FORMAT,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 2,
        .video_timing =
            {
                .h_size = 800,
                .v_size = 800,
                .hsync_pulse_width = 20,
                .hsync_back_porch = 20,
                .hsync_front_porch = 40,
                .vsync_pulse_width = 4,
                .vsync_back_porch = 12,
                .vsync_front_porch = 24,
            },
        .flags =
            {
                .use_dma2d = true,
                .disable_lp = false,
            },
    };
    static jd9365_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .mipi_config =
            {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
                .lane_num = TEST_MIPI_DSI_LANE_NUM,
            },
    };
    static const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .flags =
            {
                .reset_active_high = 0,
            },
        .vendor_config = &vendor_config,
    };

    auto out = std::make_unique<DisplayJd9365>(mipi_dbi_io, panel_config);

    return out;
}

} // namespace

extern "C" int rust_main(void* wifi_handle);
#include "hal/c_interface/i_wifi.h"
struct Networks ScanForNetworks(IWifiHandle *handle)
{
    return {};
}

void ConnectToHotspot(IWifiHandle *handle, const char* ssid, const char* password)
{
    printf("Connecting with %p, to SSID: %s:%s\n", handle, ssid, password);
}


extern "C" void
app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ApplicationState application_state;

    gpio_config_t io_conf = {};

    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask =
        (1ull << kTftBacklight) | (1ull << kPinLeftBuzzer) | (1ull << kPinRightBuzzer);
    gpio_config(&io_conf);

    // Turn on the backlight
    gpio_set_level(kTftBacklight, 0);


    auto display = CreateDisplay();

    sdmmc_host_t sd_mmc_host_config = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    sd_mmc_host_config.slot = SDMMC_HOST_SLOT_0;
    sd_mmc_host_config.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

#define CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID 4
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
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

    // Devices / helper classes
    auto left_buzzer_gpio = std::make_unique<TargetGpio>(kPinLeftBuzzer);
    auto right_buzzer_gpio = std::make_unique<TargetGpio>(kPinRightBuzzer);

    left_buzzer_gpio->SetState(true);
    right_buzzer_gpio->SetState(true);
    os::Sleep(500ms);
    left_buzzer_gpio->SetState(false);
    right_buzzer_gpio->SetState(false);

    auto image_cache = std::make_unique<ImageCache>();
    //    auto uart1 = std::make_unique<TargetUart>(UART_NUM_1,
    //                                              9600,
    //                                              GPIO_NUM_44,  // RX
    //                                              GPIO_NUM_43); // TX
    //

    auto gps = std::make_unique<I2cGps>(kI2cSclPin, kI2cSdaPin);
    //auto uart_gps = std::make_unique<UartGps>(*uart1);
    auto filesystem = std::make_unique<Filesystem>("/sdcard/app_data/");
    auto wifi_client = std::make_unique<WifiClientEsp32>(application_state);

    auto ssid_data = filesystem->ReadFile("SSID.TXT");
    if (ssid_data)
    {
        std::stringstream ssid_stream(reinterpret_cast<const char*>(ssid_data->data()));
        std::string ssid, password;

        std::getline(ssid_stream, ssid);
        std::getline(ssid_stream, password);

        wifi_client->Start(ssid.c_str(), password.c_str());
    }

    auto httpd_client = std::make_unique<HttpdClient>();

    auto pm = std::make_unique<PmEsp32>();

    auto can = std::make_unique<CanEsp32>(GPIO_NUM_5, GPIO_NUM_4, 500000);

    // Threads
    /*
    auto buzz_handler =
        std::make_unique<BuzzHandler>(*left_buzzer_gpio, *right_buzzer_gpio, application_state);
    //auto ble_server = std::make_unique<BleServerEsp32>();
    auto ble_server = std::make_unique<BleServerHost>();
    //auto app_simulator = std::make_unique<AppSimulator>(*ble_server);
    auto can_bus_handler = std::make_unique<CanBusHandler>(*can, application_state, 0x5e);

    auto gps_reader = std::make_unique<GpsReader>(*gps);
    auto tile_cache = std::make_unique<TileCache>(application_state,
                                                  pm->CreateFullPowerLock(),
                                                  gps_reader->AttachListener(),
                                                  *filesystem,
                                                  *httpd_client);
    auto ble_handler = std::make_unique<BleHandler>(*ble_server, application_state, *image_cache);
    auto user_interface = std::make_unique<UserInterface>(*display,
                                                          pm->CreateFullPowerLock(),
                                                          application_state,
                                                          gps_reader->AttachListener(),
                                                          *image_cache,
                                                          *tile_cache);
                                                          */

                                                          int vobb;
    auto v = rust_main((void*)&vobb);
    printf("Rust returned %d. Vobb is %p\n", v, &vobb);

    //    buzz_handler->Start("buzz_handler", 8192);
    //app_simulator->Start("app_simulator", 8192);
    //    can_bus_handler->Start("can_bus_handler", 4096);
    //    ble_handler->Start("ble_server", 8192);
    //    gps_reader->Start("gps_reader", 8192);
    //    tile_cache->Start("tile_cache", 8192);
    //    user_interface->Start("user_interface", os::ThreadCore::kCore1, 8192);

    while (true)
    {
        vTaskSuspend(nullptr);
    }
}
