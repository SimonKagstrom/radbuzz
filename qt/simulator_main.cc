#include "app_simulator.hh"
#include "ble_client_host.hh"
#include "ble_handler.hh"
#include "ble_server_host.hh"
#include "ble_server_qt.hh"
#include "blitter_host.hh"
#include "buzz_handler.hh"
#include "filesystem.hh"
#include "gps_reader.hh"
#include "https_client.hh"
#include "input.hh"
#include "job_pool_thread.hh"
#include "nvm_host.hh"
#include "opportunistic_scheduler.hh"
#include "ota_updater.hh"
#include "ota_updater_host.hh"
#include "pm_host.hh"
#include "simulator_mainwindow.hh"
#include "speedometer_handler.hh"
#include "storage.hh"
#include "temperature_monitor.hh"
#include "tile_cache.hh"
#include "time.hh"
#include "trip_computer.hh"
#include "user_interface.hh"
#include "wgs84_to_osm_point.hh"
#include "wifi_client_host.hh"
#include "wifi_handler.hh"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <stdlib.h>

int
main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCommandLineParser parser;

    parser.addOptions({
        {{"s", "seed"}, "Random seed", "seed"},
        {{"u", "updated"}, "Set the application updated flag"},
        {{"b", "ble"}, "Use real BLE (e.g., for Gadgetbridge) instead of the demo mode"},
    });

    parser.process(a);

    int seed = 0;
    if (parser.isSet("seed"))
    {
        seed = parser.value("seed").toInt();
    }
    bool updated = false;
    if (parser.isSet("updated"))
    {
        updated = true;
    }
    const auto use_ble = parser.isSet("ble");

    auto scheduler = std::make_unique<os::OpportunisticSchedulerThread>();
    scheduler->Start("scheduler");

    ApplicationState application_state;
    PostOffice<MSG::AllMessages> post_office;

    auto rw = application_state.CheckoutReadWrite();

    rw.Set<AS::wifi_connected>(true);
    rw.Set<AS::demo_mode>(!use_ble);
    // Stored by VESC, so update to 100km + some random number here
    rw.Set<AS::odometer>(100 * 1000 + rand() % 2000);

    auto wifi_client = std::make_unique<WifiClientHost>();

    MainWindow window(application_state, post_office, *wifi_client);

    srand(seed);

    auto job_pool = std::make_unique<JobPoolThread>();

    // Devices / helper classes
    std::unique_ptr<hal::IBleServer> ble_server;
    if (use_ble)
    {
        // Gadgetbridge recognizes the device as a Bangle.js from the name prefix
        ble_server = std::make_unique<BleServerQt>("Bangle.js radbuzz_qt");
    }
    else
    {
        ble_server = std::make_unique<BleServerHost>();
    }
    auto ble_client = std::make_unique<BleClientHost>();
    auto image_cache = std::make_unique<ImageCache>();
    auto filesystem = std::make_unique<Filesystem>("./app_data");
    auto https_client = std::make_unique<HttpsClient>();
    auto pm = std::make_unique<PmHost>();
    auto nvm_host = std::make_unique<NvmHost>("nvm.txt");
    auto blitter = std::make_unique<BlitterHost>();
    auto ota_updater = std::make_unique<OtaUpdaterHost>(false);

    // Threads
    auto storage = std::make_unique<Storage>(application_state, *nvm_host);
    auto ota_updater_thread = std::make_unique<OtaUpdater>(*ota_updater, application_state);
    auto wifi_handler = std::make_unique<WifiHandler>(application_state, *filesystem, *wifi_client);
    auto input = std::make_unique<Input>(window.GetButtonGpio(), window, window.GetTouch());
    auto trip_computer = std::make_unique<TripComputer>(application_state, post_office);
    auto app_simulator = std::make_unique<AppSimulator>(application_state, post_office);
    auto tile_cache = std::make_unique<TileCache>(
        application_state, post_office, pm->CreateFullPowerLock(), *filesystem, *https_client);
    auto ble_handler =
        std::make_unique<BleHandler>(
            *ble_server, *ble_client, application_state, post_office, *image_cache);
    auto buzz_handler = std::make_unique<BuzzHandler>(
        window.GetLeftBuzzer(), window.GetRightBuzzer(), application_state);
    auto temperature_monitor = std::make_unique<TemperatureMonitor>(application_state);
    auto user_interface = std::make_unique<UserInterface>(window.GetDisplay(),
                                                          *blitter,
                                                          pm->CreateFullPowerLock(),
                                                          *input, // IInput
                                                          *ota_updater_thread,
                                                          application_state,
                                                          post_office,
                                                          *image_cache,
                                                          *tile_cache,
                                                          *trip_computer);

    auto speedometer_handler =
        std::make_unique<SpeedometerHandler>(window.GetStepperMotor(), application_state, 6000);

    wifi_handler->Start("wifi_handler");
    input->Start("input");
    ble_handler->Start("ble_handler");
    buzz_handler->Start("buzz_handler");
    tile_cache->Start("tile_cache");
    user_interface->Start("user_interface");
    speedometer_handler->Start("speedometer_handler");

    os::Sleep(10ms);
    job_pool->AttachPooledThread(std::move(storage));
    job_pool->AttachPooledThread(std::move(trip_computer));
    job_pool->AttachPooledThread(std::move(ota_updater_thread));
    job_pool->AttachPooledThread(std::move(app_simulator));
    job_pool->AttachPooledThread(std::move(temperature_monitor));

    job_pool->Start("job_pool");


    window.show();

    auto out = QApplication::exec();

    // Workaround a hang on exit. The target application never exits
    exit(0);

    return out;
}
