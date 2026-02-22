#include "app_simulator.hh"
#include "ble_handler.hh"
#include "ble_server_host.hh"
#include "buzz_handler.hh"
#include "filesystem.hh"
#include "gps_reader.hh"
#include "httpd_client.hh"
#include "pm_host.hh"
#include "simulator_mainwindow.hh"
#include "tile_cache.hh"
#include "time.hh"
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

    ApplicationState application_state;
    application_state.CheckoutReadWrite().Set<AS::wifi_connected>(true);

    MainWindow window(application_state);

    srand(seed);

    // Devices / helper classes
    auto ble_server = std::make_unique<BleServerHost>();
    auto image_cache = std::make_unique<ImageCache>();
    auto filesystem = std::make_unique<Filesystem>("./app_data");
    auto httpd_client = std::make_unique<HttpdClient>();
    auto pm = std::make_unique<PmHost>();

    // Threads
    auto app_simulator = std::make_unique<AppSimulator>(application_state, *ble_server);
    auto gps_reader =
        std::make_unique<GpsReader>(application_state, app_simulator->GetSimulatedGps());
    auto tile_cache = std::make_unique<TileCache>(
        application_state, pm->CreateFullPowerLock(), *filesystem, *httpd_client);
    auto ble_handler = std::make_unique<BleHandler>(*ble_server, application_state, *image_cache);
    auto buzz_handler = std::make_unique<BuzzHandler>(
        window.GetLeftBuzzer(), window.GetRightBuzzer(), application_state);
    auto user_interface = std::make_unique<UserInterface>(window.GetDisplay(),
                                                          pm->CreateFullPowerLock(),
                                                          application_state,
                                                          *image_cache,
                                                          *tile_cache);

    gps_reader->Start("gps_reader");
    ble_handler->Start("ble_handler");
    buzz_handler->Start("buzz_handler");
    tile_cache->Start("tile_cache");
    user_interface->Start("user_interface");

    os::Sleep(10ms);
    app_simulator->Start("app_simulator");

    window.show();

    auto out = QApplication::exec();

    // Workaround a hang on exit. The target application never exits
    exit(0);

    return out;
}
