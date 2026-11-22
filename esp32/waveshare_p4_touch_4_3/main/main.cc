#include "app_simulator.hh"
#include "backlight_ledc_esp32.hh"
#include "ble_handler.hh"
#include "ble_server_esp32.hh"
#include "blitter_esp32.hh"
#include "brightness_controller.hh"
#include "button_debouncer.hh"
#include "buzz_handler.hh"
#include "can_esp32.hh"
#include "clock_handler.hh"
#include "filesystem.hh"
#include "gpio_esp32.hh"
#include "gps_reader.hh"
#include "httpd_ota_updater_esp32.hh"
#include "i2c_gps_esp32.hh"
#include "image_cache.hh"
#include "input.hh"
#include "job_pool_thread.hh"
#include "nvm_esp32.hh"
#include "ota_updater.hh"
#include "pm_esp32.hh"
#include "rotary_encoder.hh"
#include "sdkconfig.h"
#include "speedometer_handler.hh"
#include "st7701_display_esp32.hh"
#include "st7701_panel_waveshare_4_3.hh"
#include "stepper_motor_esp32.hh"
#include "storage.hh"
#include "temperature_monitor.hh"
#include "touch_esp32.hh"
#include "traccar_uploader.hh"
#include "trip_compactor.hh"
#include "trip_computer.hh"
#include "trip_recorder.hh"
#include "uart_esp32.hh"
#include "uart_gps_esp32.hh"
#include "user_interface.hh"
#include "vesc_can_bus_handler.hh"
#include "wifi_client_esp32.hh"
#include "wifi_handler.hh"

#include <algorithm>
#include <driver/sdmmc_host.h>
#include <esp_app_format.h>
#include <esp_hosted.h>
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
constexpr auto kBacklightPolarity = BacklightLedcEsp32::Polarity::kActiveLow;

constexpr auto kPinLeftBuzzer = GPIO_NUM_25;  // TODO
constexpr auto kPinRightBuzzer = GPIO_NUM_49; // TODO

constexpr auto kPinStepperSleepGpio = GPIO_NUM_30;
constexpr auto kPinStepperDirGpio = GPIO_NUM_46;
constexpr auto kPinStepGpio = GPIO_NUM_31;

constexpr auto kCanBusTxPin = GPIO_NUM_47;
constexpr auto kCanBusRxPin = GPIO_NUM_48;

constexpr auto kI2cSdaPin = GPIO_NUM_7;
constexpr auto kI2cSclPin = GPIO_NUM_8;

constexpr auto kGpsUartTxPin = GPIO_NUM_46;
constexpr auto kGpsUartRxPin = GPIO_NUM_32;

constexpr auto kButtonGpio = GPIO_NUM_50;
constexpr auto kRotaryEncoderPinA = GPIO_NUM_51;
constexpr auto kRotaryEncoderPinB = GPIO_NUM_52;

constexpr auto kSdPwrCtrlLdoIoId = 4;

constexpr auto kLcdResetPin = GPIO_NUM_27;
constexpr auto kMipiDsiPhyPwrLdoChan = 3; // LDO_VO3 is connected to VDD_MIPI_DPHY
constexpr auto kMipiDsiPhyPwrLdoVoltageMv = 2500;

constexpr const char* kC6FwPartitionLabel = "c6fw";
constexpr size_t kSlaveOtaChunkSize = 1500;

struct PartitionFirmwareInfo
{
    size_t image_size {0};
};

esp_err_t
CheckPartitionHasData(const esp_partition_t* partition)
{
    uint8_t probe[256] {};
    size_t offset = 0;

    while (offset < 1024 && offset < partition->size)
    {
        const size_t remain_probe = static_cast<size_t>(1024) - offset;
        const size_t remain_part = static_cast<size_t>(partition->size) - offset;
        const size_t remain = remain_probe < remain_part ? remain_probe : remain_part;
        const size_t read_len = sizeof(probe) < remain ? sizeof(probe) : remain;
        const auto ret = esp_partition_read(partition, offset, probe, read_len);
        if (ret != ESP_OK)
        {
            return ret;
        }

        const bool all_ff = std::all_of(std::begin(probe),
                                        std::begin(probe) + static_cast<std::ptrdiff_t>(read_len),
                                        [](uint8_t b) { return b == 0xFF; });
        if (!all_ff)
        {
            return ESP_OK;
        }

        offset += read_len;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t
ParseFirmwareInfoFromPartition(const esp_partition_t* partition, PartitionFirmwareInfo& info)
{
    esp_image_header_t image_header {};
    auto ret = esp_partition_read(partition, 0, &image_header, sizeof(image_header));
    if (ret != ESP_OK)
    {
        return ret;
    }
    if (image_header.magic != ESP_IMAGE_HEADER_MAGIC)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t offset = sizeof(image_header);
    size_t total_size = sizeof(image_header);

    for (int i = 0; i < image_header.segment_count; ++i)
    {
        esp_image_segment_header_t segment_header {};
        ret = esp_partition_read(partition, offset, &segment_header, sizeof(segment_header));
        if (ret != ESP_OK)
        {
            return ret;
        }

        total_size += sizeof(segment_header) + segment_header.data_len;
        offset += sizeof(segment_header) + segment_header.data_len;
    }

    total_size += (16 - (total_size % 16)) % 16; // image alignment
    total_size += 1;                             // checksum byte
    if (image_header.hash_appended == 1)
    {
        total_size += (16 - (total_size % 16)) % 16;
        total_size += 32; // SHA256
    }

    if (total_size > partition->size)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    info.image_size = total_size;
    return ESP_OK;
}

bool
PerformC6SlaveOtaFromPartition(bool force)
{
    if (force)
    {
        printf("C6 OTA: forced update requested\n");
    }

    // Check the version first, since reading the partition disturbs the display (it stalls the
    // caches and the other core), so don't do that unless an update is needed
    esp_hosted_coprocessor_fwver_t current_fw {};
    const int fwver_ret = esp_hosted_get_coprocessor_fwversion(&current_fw);
    if (fwver_ret != ESP_OK)
    {
        printf("C6 OTA: failed to read current C6 fw version (%s)\n", esp_err_to_name(fwver_ret));
    }

    if (current_fw.major1 == ESP_HOSTED_VERSION_MAJOR_1 &&
        current_fw.minor1 == ESP_HOSTED_VERSION_MINOR_1 &&
        current_fw.patch1 == ESP_HOSTED_VERSION_PATCH_1)
    {
        printf("C6 OTA: already up to date (%lu.%lu.%lu)\n",
               current_fw.major1,
               current_fw.minor1,
               current_fw.patch1);

        if (!force)
        {
            return false;
        }
    }

    const auto* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, kC6FwPartitionLabel);
    if (partition == nullptr)
    {
        printf("C6 OTA: partition '%s' not found\n", kC6FwPartitionLabel);
        return false;
    }

    auto ret = CheckPartitionHasData(partition);
    if (ret != ESP_OK)
    {
        printf("C6 OTA: partition '%s' is empty or unreadable (%s)\n",
               kC6FwPartitionLabel,
               esp_err_to_name(ret));
        return false;
    }

    PartitionFirmwareInfo fw_info {};
    ret = ParseFirmwareInfoFromPartition(partition, fw_info);
    if (ret != ESP_OK)
    {
        printf("C6 OTA: invalid firmware in partition '%s' (%s)\n",
               kC6FwPartitionLabel,
               esp_err_to_name(ret));
        return false;
    }

    printf("C6 OTA: update required current=%lu.%lu.%lu target=%u.%u.%u\n",
           current_fw.major1,
           current_fw.minor1,
           current_fw.patch1,
           ESP_HOSTED_VERSION_MAJOR_1,
           ESP_HOSTED_VERSION_MINOR_1,
           ESP_HOSTED_VERSION_PATCH_1);

    ret = esp_hosted_slave_ota_begin();
    if (ret != ESP_OK)
    {
        printf("C6 OTA: begin failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    static std::array<uint8_t, kSlaveOtaChunkSize> chunk {};
    size_t offset = 0;
    while (offset < fw_info.image_size)
    {
        const size_t remaining = fw_info.image_size - offset;
        const size_t chunk_len = chunk.size() < remaining ? chunk.size() : remaining;
        ret = esp_partition_read(partition, offset, chunk.data(), chunk_len);
        if (ret != ESP_OK)
        {
            printf("C6 OTA: partition read failed at %u: %s\n",
                   static_cast<unsigned int>(offset),
                   esp_err_to_name(ret));
            esp_hosted_slave_ota_end();
            return false;
        }

        ret = esp_hosted_slave_ota_write(chunk.data(), chunk_len);
        if (ret != ESP_OK)
        {
            printf("C6 OTA: write failed at %u: %s\n",
                   static_cast<unsigned int>(offset),
                   esp_err_to_name(ret));
            esp_hosted_slave_ota_end();
            return false;
        }

        offset += chunk_len;
    }

    ret = esp_hosted_slave_ota_end();
    if (ret != ESP_OK)
    {
        printf("C6 OTA: end failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    ret = esp_hosted_slave_ota_activate();
    if (ret != ESP_OK)
    {
        printf("C6 OTA: activate failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    printf("C6 OTA: update completed and activated\n");
    return true;
}

} // namespace

// esp_hosted calls esp_hosted_init() from a constructor, i.e., before the scheduler is started.
// At that point, only a small part of the internal RAM is available as heap, and esp_hosted
// allocations there starve the IDF startup init functions (sleep_clock_icg_startup_init aborts).
// Skip the constructor call (via -Wl,--wrap) and init from app_main instead.
extern "C" int __real_esp_hosted_init(void);

extern "C" int
__wrap_esp_hosted_init(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return ESP_OK;
    }

    return __real_esp_hosted_init();
}

extern "C" void
app_main(void)
{
    ESP_ERROR_CHECK(esp_hosted_init());

    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Avoid stack allocation
    static ApplicationState application_state;
    static PostOffice<MSG::AllMessages> post_office;

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


    // Keep the backlight off until there is something on the display
    auto backlight = std::make_unique<BacklightLedcEsp32>(
        BacklightLedcEsp32::Config {.pin = kTftBacklight, .polarity = kBacklightPolarity});

    auto display = std::make_unique<DisplaySt7701>(
        st7701::GetWaveshare4_3Panel(),
        DisplaySt7701::BoardConfig {.reset_pin = kLcdResetPin,
                                    .phy_ldo_channel = kMipiDsiPhyPwrLdoChan,
                                    .phy_ldo_voltage_mv = kMipiDsiPhyPwrLdoVoltageMv});

    // Start with the background color, and let the UI draw on top of that. Flip, since that
    // also writes the frame buffer back from the cache
    std::fill_n(display->GetFrameBuffer(hal::IDisplay::Owner::kSoftware),
                hal::kDisplayWidth * hal::kDisplayHeight,
                lv_color_to_u16(UserInterface::GetBackgroundColor()));
    display->Flip();
    // Until the brightness controller takes over
    backlight->SetBrightness(100);

    auto nvm = std::make_unique<NvmEsp32>();
    // Loads the configuration in the constructor, run in the job pool (below)
    auto storage = std::make_unique<Storage>(application_state, *nvm);


    // The user interface first, so that the display comes up quickly. The rest (C6, SD card,
    // BLE etc) is setup afterwards
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

    auto image_cache = std::make_unique<ImageCache>();
    auto filesystem = std::make_unique<Filesystem>("/sdcard/app_data/");

    auto https_client = std::make_unique<HttpsClient>();

    auto blitter = std::make_unique<BlitterEsp32>();
    auto pm = std::make_unique<PmEsp32>();
    auto pin_a = std::make_unique<GpioEsp32>(kRotaryEncoderPinA);
    auto pin_b = std::make_unique<GpioEsp32>(kRotaryEncoderPinB);

    auto touch = std::make_unique<TouchEsp32>(i2c_mst_config, GPIO_NUM_NC);
    auto rotary_encoder = std::make_unique<RotaryEncoder>(*pin_a, *pin_b);
    auto button_debouncer = std::make_unique<ButtonDebouncer>();
    auto debounced_button = button_debouncer->AddButton(
        std::make_unique<GpioEsp32>(kButtonGpio, GpioEsp32::Polarity::kActiveLow));

    auto input = std::make_unique<Input>(*debounced_button, *rotary_encoder, *touch);

    auto tile_cache = std::make_unique<TileCache>(
        application_state, post_office, pm->CreateFullPowerLock(), *filesystem, *https_client);

    auto trip_computer = std::make_unique<TripComputer>(application_state, post_office);

    // Over the regular Wifi connection (started from the menu)
    auto httpd_ota_updater =
        std::make_unique<TargetHttpdOtaUpdater>(*display, TargetHttpdOtaUpdater::Mode::kStation);
    auto ota_updater = std::make_unique<OtaUpdater>(*httpd_ota_updater, application_state);

    auto user_interface = std::make_unique<UserInterface>(*display,
                                                          *blitter,
                                                          pm->CreateFullPowerLock(),
                                                          *input,
                                                          *ota_updater,
                                                          application_state,
                                                          post_office,
                                                          *image_cache,
                                                          *tile_cache,
                                                          *trip_computer);


    user_interface->Start("user_interface", os::ThreadCore::kCore1, 8192);
    input->Start("input");
    button_debouncer->Start("button_debouncer", os::ThreadPriority::kHigh);


    // Create before SD card (see below)
    auto wifi_client = std::make_unique<WifiClientEsp32>();

    os::Sleep(3s);
    auto force_upgrade = application_state.Get<AS::configuration>()->force_c6_update;
    application_state.CheckoutPartialSnapshot<AS::configuration>()
        .GetWritableReference<AS::configuration>()
        .force_c6_update = false;
    if (PerformC6SlaveOtaFromPartition(force_upgrade))
    {
        // Allow remote coprocessor reboot to settle, then restart host to resync transport.
        os::Sleep(2s);
        esp_restart();
    }

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

    esp_hosted_bt_controller_init();
    esp_hosted_bt_controller_enable();

    // Devices / helper classes
    //auto left_buzzer_gpio = std::make_unique<GpioEsp32>(kPinLeftBuzzer);
    //auto right_buzzer_gpio = std::make_unique<GpioEsp32>(kPinRightBuzzer);
    auto uart_gps = std::make_unique<UartEsp32>(UART_NUM_2,
                                                9600,
                                                kGpsUartRxPin,  // RX
                                                kGpsUartTxPin); // TX
    auto gps = std::make_unique<UartGps>(*uart_gps);
    auto can = std::make_unique<CanEsp32>(kCanBusTxPin, kCanBusRxPin, 500000);
    //auto stepper_sleep_gpio =
    //    std::make_unique<GpioEsp32>(kPinStepperSleepGpio, GpioEsp32::Polarity::kActiveHigh);
    //auto stepper_dir_gpio =
    //    std::make_unique<GpioEsp32>(kPinStepperDirGpio, GpioEsp32::Polarity::kActiveLow);
    //    auto stepper_motor =
    //        std::make_unique<StepperMotorEsp32>(*stepper_sleep_gpio, *stepper_dir_gpio, kPinStepGpio);
    //

    //    stepper_motor->Start();

    auto job_pool = std::make_unique<JobPoolThread>();

    // Threads
    //  auto buzz_handler =
    //      std::make_unique<BuzzHandler>(*left_buzzer_gpio, *right_buzzer_gpio, application_state);


    //    constexpr auto kFullRotation = 2400;
    //    auto speedometer_handler =
    //        std::make_unique<SpeedometerHandler>(*stepper_motor, application_state, kFullRotation);

    // application_state.Set<AS::demo_mode>(true);

    auto vesc_can_bus_handler = std::make_unique<VescCanBusHandler>(*can, application_state);

    auto gps_reader = std::make_unique<GpsReader>(application_state, *gps);
    auto trip_recorder = std::make_unique<TripRecorder>(application_state, post_office, *filesystem);
    auto trip_compactor = std::make_unique<TripCompactor>(post_office, *filesystem);
    // A client of its own, since a client is a single connection, used from one thread
    auto traccar_https_client = std::make_unique<HttpsClient>();
    auto traccar_uploader = std::make_unique<TraccarUploader>(
        application_state, post_office, *filesystem, *traccar_https_client);
    auto temperature_monitor = std::make_unique<TemperatureMonitor>(application_state);
    auto clock_handler = std::make_unique<ClockHandler>(application_state, post_office);
    auto brightness_controller = std::make_unique<BrightnessController>(
        *backlight,
        application_state,
        BrightnessController::Levels {.night = 10, .dusk = 30, .low_sun = 60, .day = 100},
        HomePositionOrDefault(*application_state.Get<AS::configuration>()));

    auto ble_server = std::make_unique<BleServerEsp32>();
    auto app_simulator = std::make_unique<AppSimulator>(application_state, post_office);
    auto wifi_handler =
        std::make_unique<WifiHandler>(application_state, post_office, *filesystem, *wifi_client);
    auto ble_handler = std::make_unique<BleHandler>(
        *ble_server, *ble_server, application_state, post_office, *image_cache);


    //  buzz_handler->Start("buzz_handler", 8192);
    vesc_can_bus_handler->Start("vesc_can_bus_handler", 4096);
    ble_handler->Start("ble_server", 8192);
    wifi_handler->Start("wifi_handler", 8192);
    //speedometer_handler->Start("speedometer_handler");

    tile_cache->Start("tile_cache", 8192);
    gps_reader->Start("gps_reader");
    trip_recorder->Start("trip_recorder", 4096);
    // Large enough for the TLS handshake, like the tile cache
    traccar_uploader->Start("traccar_uploader", 8192);

    // References to these are kept elsewhere (e.g., by the UI), but the job pool owns them
    job_pool->AttachPooledThread(std::move(storage));
    job_pool->AttachPooledThread(std::move(trip_computer));
    job_pool->AttachPooledThread(std::move(ota_updater));
    job_pool->AttachPooledThread(std::move(app_simulator));
    job_pool->AttachPooledThread(std::move(temperature_monitor));
    job_pool->AttachPooledThread(std::move(clock_handler));
    job_pool->AttachPooledThread(std::move(brightness_controller));
    job_pool->AttachPooledThread(std::move(trip_compactor));

    job_pool->Start("job_pool", 8192);


    while (true)
    {
        vTaskSuspend(nullptr);
    }
}
