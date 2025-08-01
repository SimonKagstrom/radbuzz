#include "app_simulator.hh"
#include "ble_handler.hh"
#include "ble_server_host.hh"
#include "simulator_mainwindow.hh"
#include "time.hh"
#include "user_interface.hh"

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
    auto user_interface =
        std::make_unique<UserInterface>(window.GetDisplay(), application_state, *image_cache);


    application_state.Checkout()->current_icon_hash = 0x27d9a40f;

    ble_handler->Start("ble_handler");
    user_interface->Start("user_interface");

    os::Sleep(10ms);
    app_simulator->Start("app_simulator");

    window.show();

    auto out = QApplication::exec();

    // Workaround a hang on exit. The target application never exits
    exit(0);

    return out;
}
