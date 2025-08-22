#include "app_simulator.hh"
#include "ble_handler.hh"
#include "ble_server_host.hh"
#include "buzz_handler.hh"
#include "gps_reader.hh"
#include "httpd_client.hh"
#include "simulator_mainwindow.hh"
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

    MainWindow window;

    ApplicationState application_state;

    srand(seed);

    // Devices / helper classes
    auto ble_server = std::make_unique<BleServerHost>();
    auto image_cache = std::make_unique<ImageCache>();

    // Threads
    auto app_simulator = std::make_unique<AppSimulator>(*ble_server);
    auto ble_handler = std::make_unique<BleHandler>(*ble_server, application_state, *image_cache);
    auto buzz_handler = std::make_unique<BuzzHandler>(
        window.GetLeftBuzzer(), window.GetRightBuzzer(), application_state);
    auto user_interface =
        std::make_unique<UserInterface>(window.GetDisplay(), application_state, *image_cache);

    ble_handler->Start("ble_handler");
    buzz_handler->Start("buzz_handler");
    user_interface->Start("user_interface");

    os::Sleep(10ms);
    app_simulator->Start("app_simulator");

    window.show();

    GpsPosition p;
    p.latitude = 59.29325147850288;
    p.longitude = 17.956672660463134;
    //p.latitude = 59.34451772083831;
    //p.longitude = 18.047964506090967;

    auto x = Wgs84ToOsmPoint(p, 15);
    auto t = ToTile(*x);

    printf("pixel pos: %d,%d\n", x->x, x->y);
    printf("tile pos: %d,%d -> offset %d,%d\n",
           t.x,
           t.y,
           x->x - t.x * kTileSize,
           x->y - t.y * kTileSize);

    constexpr auto kalle = OSM_API_KEY;
    auto vobb = std::make_unique<HttpdClient>();
    auto mibb = vobb->Get(std::format(
        "https://tile.thunderforest.com/cycle/15/{}/{}.png?apikey={}", t.x, t.y, kalle));
    if (mibb)
    {
        auto out_file = QFile("test.png");
        if (out_file.open(QIODevice::WriteOnly))
        {
            out_file.write(reinterpret_cast<const char*>(mibb->data()), mibb->size());
            out_file.close();
        }
        else
        {
            std::print("Failed to open file for writing\n");
        }
    }

    auto out = QApplication::exec();

    // Workaround a hang on exit. The target application never exits
    exit(0);

    return out;
}
