#include "app_simulator.hh"
#include "ble_handler.hh"
#include "ble_server_host.hh"
#include "blitter_host.hh"
#include "buzz_handler.hh"
#include "filesystem.hh"
#include "gps_reader.hh"
#include "httpd_client.hh"
#include "input.hh"
#include "nvm_host.hh"
#include "opportunistic_scheduler.hh"
#include "pm_host.hh"
#include "simulator_mainwindow.hh"
#include "speedometer_handler.hh"
#include "storage.hh"
#include "tile_cache.hh"
#include "time.hh"
#include "trip_computer.hh"
#include "user_interface.hh"
#include "wgs84_to_osm_point.hh"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <print>
#include <stdlib.h>

int
main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCommandLineParser parser;

    parser.addOptions({
        {{"s", "seed"}, "Random seed", "seed"},
        {{"u", "updated"}, "Set the application updated flag"},
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

    auto scheduler = std::make_unique<os::OpportunisticSchedulerThread>();
    scheduler->Start("scheduler");

    ApplicationState application_state;
    auto rw = application_state.CheckoutReadWrite();

    rw.Set<AS::wifi_connected>(true);
    rw.Set<AS::demo_mode>(true);

    MainWindow window(application_state);

    srand(seed);

    // Devices / helper classes
    auto ble_server = std::make_unique<BleServerHost>();
    auto image_cache = std::make_unique<ImageCache>();
    auto filesystem = std::make_unique<Filesystem>("./app_data");
    auto httpd_client = std::make_unique<HttpdClient>();
    auto pm = std::make_unique<PmHost>();
    auto nvm_host = std::make_unique<NvmHost>("nvm.txt");
    auto blitter = std::make_unique<BlitterHost>();

    // Threads
    auto storage = std::make_unique<Storage>(application_state, *nvm_host);
    auto input = std::make_unique<Input>(window.GetButtonGpio(), window, window.GetTouch());
    auto trip_computer = std::make_unique<TripComputer>(application_state);
    auto app_simulator = std::make_unique<AppSimulator>(application_state, *ble_server);
    auto tile_cache = std::make_unique<TileCache>(
        application_state, pm->CreateFullPowerLock(), *filesystem, *httpd_client);
    auto ble_handler = std::make_unique<BleHandler>(*ble_server, application_state, *image_cache);
    auto buzz_handler = std::make_unique<BuzzHandler>(
        window.GetLeftBuzzer(), window.GetRightBuzzer(), application_state);
    auto user_interface = std::make_unique<UserInterface>(window.GetDisplay(),
                                                          *blitter,
                                                          pm->CreateFullPowerLock(),
                                                          *input, // IInput
                                                          application_state,
                                                          *image_cache,
                                                          *tile_cache);

    auto speedometer_handler =
        std::make_unique<SpeedometerHandler>(window.GetStepperMotor(), application_state, 6000);

    storage->Start("storage");
    input->Start("input");
    trip_computer->Start("trip_computer");
    ble_handler->Start("ble_handler");
    buzz_handler->Start("buzz_handler");
    tile_cache->Start("tile_cache");
    user_interface->Start("user_interface");
    speedometer_handler->Start("speedometer_handler");

    os::Sleep(10ms);
    app_simulator->Start("app_simulator");

    window.show();

    auto out = QApplication::exec();

    // Workaround a hang on exit. The target application never exits
    exit(0);

    return out;
}
