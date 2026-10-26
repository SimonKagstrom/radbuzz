#include "test.hh"
#include "wifi_network_list.hh"

namespace
{

using Type = MSG::WifiType;

std::vector<std::string>
Ssids(const std::vector<WifiNetworkListEntry>& list)
{
    std::vector<std::string> ssids;

    for (const auto& entry : list)
    {
        ssids.push_back(entry.ssid);
    }

    return ssids;
}

} // namespace

TEST_SUITE_BEGIN("wifi_network_list");

TEST_CASE("the wifi network list is ordered connected, known, others")
{
    const std::vector<MSG::WifiScanEntry> scan {
        {"Neighbour", -70, Type::kSecured},
        {"Office", -80, Type::kSecured},
        {"Cafe", -50, Type::kOpen},
        {"Home", -60, Type::kSecured},
        {"Garage", -40, Type::kSecured},
        {"Library", -55, Type::kOpen},
    };
    const std::vector<WifiSsidNetwork> known {
        {"Home", "home-password"},
        {"Office", "office-password"},
        {"Garage", "garage-password"},
        {"Library", ""},
    };

    WHEN("connected")
    {
        auto list = CreateWifiNetworkList(scan, known, "Home");

        THEN("the connected network is first, then known and others by signal strength")
        {
            REQUIRE(Ssids(list) == std::vector<std::string> {
                                       "Home", "Garage", "Library", "Office", "Cafe", "Neighbour"});
            REQUIRE(list[0].connected);
            REQUIRE(std::ranges::count_if(list, &WifiNetworkListEntry::connected) == 1);
        }

        THEN("the known networks have their stored password")
        {
            REQUIRE(list[0].password == "home-password");
            REQUIRE(list[1].password == "garage-password");
            // Open, but known
            REQUIRE(list[2].password == "");
            REQUIRE(list[3].password == "office-password");
            REQUIRE(list[4].password == std::nullopt);
            REQUIRE(list[5].password == std::nullopt);
        }

        THEN("the scan data is kept")
        {
            REQUIRE(list[0] == WifiNetworkListEntry {"Home", -60, Type::kSecured, true, "home-password"});
            REQUIRE(list[4] == WifiNetworkListEntry {"Cafe", -50, Type::kOpen, false, std::nullopt});
        }
    }

    WHEN("not connected")
    {
        auto list = CreateWifiNetworkList(scan, known, "");

        THEN("the known networks are first")
        {
            REQUIRE(Ssids(list) == std::vector<std::string> {
                                       "Garage", "Library", "Home", "Office", "Cafe", "Neighbour"});
            REQUIRE(std::ranges::none_of(list, &WifiNetworkListEntry::connected));
        }
    }
}

TEST_CASE("the connected network is listed also when it's missing from the scan")
{
    const std::vector<MSG::WifiScanEntry> scan {{"Neighbour", -70, Type::kSecured}};

    WHEN("it's known")
    {
        auto list = CreateWifiNetworkList(scan, std::vector<WifiSsidNetwork> {{"Home", "pw"}}, "Home");

        THEN("it's first, without signal strength or type")
        {
            REQUIRE(list.size() == 2);
            REQUIRE(list[0] == WifiNetworkListEntry {"Home", std::nullopt, Type::kUnknown, true, "pw"});
        }
    }

    WHEN("it has been forgotten while connected")
    {
        auto list = CreateWifiNetworkList(scan, {}, "Home");

        THEN("it's still listed, but without a password")
        {
            REQUIRE(list[0].ssid == "Home");
            REQUIRE(list[0].connected);
            REQUIRE(list[0].password == std::nullopt);
        }
    }

    WHEN("the scan is empty")
    {
        auto list = CreateWifiNetworkList({}, {}, "Home");

        THEN("only the connected network is listed")
        {
            REQUIRE(Ssids(list) == std::vector<std::string> {"Home"});
        }
    }
}

TEST_CASE("known networks out of range are not listed")
{
    auto list = CreateWifiNetworkList(std::vector<MSG::WifiScanEntry> {{"Cafe", -50, Type::kOpen}},
                                      std::vector<WifiSsidNetwork> {{"Home", "pw"}, {"Office", "pw"}},
                                      "");

    REQUIRE(Ssids(list) == std::vector<std::string> {"Cafe"});
}

TEST_CASE("networks without signal strength are last in their group, in scan order")
{
    const std::vector<MSG::WifiScanEntry> scan {
        {"Unknown 1"},
        {"Weak", -90, Type::kSecured},
        {"Unknown 2"},
        // Extremes of int8_t
        {"Very weak", -128, Type::kSecured},
        {"Very strong", 127, Type::kSecured},
        // Same strength as Weak, so after it (stable)
        {"Also weak", -90, Type::kSecured},
    };

    auto list = CreateWifiNetworkList(scan, {}, "");

    REQUIRE(Ssids(list) == std::vector<std::string> {
                               "Very strong", "Weak", "Also weak", "Very weak", "Unknown 1", "Unknown 2"});
}

TEST_CASE("broken scan and configuration data is handled")
{
    WHEN("the scan has duplicate and empty SSIDs")
    {
        const std::vector<MSG::WifiScanEntry> scan {
            {"Home", -60, Type::kSecured},
            {"", -30, Type::kOpen},
            {"Home", -40, Type::kOpen},
            {"Cafe", -50, Type::kOpen},
            {"Cafe", -45, Type::kOpen},
        };

        auto list = CreateWifiNetworkList(scan, std::vector<WifiSsidNetwork> {{"Home", "pw"}}, "");

        THEN("each network is listed once, with the first data")
        {
            REQUIRE(list == std::vector<WifiNetworkListEntry> {
                                {"Home", -60, Type::kSecured, false, "pw"},
                                {"Cafe", -50, Type::kOpen, false, std::nullopt},
                            });
        }
    }

    WHEN("the connected network appears several times in the scan")
    {
        const std::vector<MSG::WifiScanEntry> scan {
            {"Home", -60, Type::kSecured},
            {"Home", -40, Type::kSecured},
        };

        auto list = CreateWifiNetworkList(scan, std::vector<WifiSsidNetwork> {{"Home", "pw"}}, "Home");

        THEN("it's only listed once")
        {
            REQUIRE(list == std::vector<WifiNetworkListEntry> {
                                {"Home", -60, Type::kSecured, true, "pw"},
                            });
        }
    }

    WHEN("a network is known twice")
    {
        auto list = CreateWifiNetworkList(
            std::vector<MSG::WifiScanEntry> {{"Home", -60, Type::kSecured}},
            std::vector<WifiSsidNetwork> {{"Home", "first"}, {"Home", "second"}},
            "");

        THEN("the first password is used, like the wifi handler does")
        {
            REQUIRE(list[0].password == "first");
        }
    }

    WHEN("everything is empty")
    {
        REQUIRE(CreateWifiNetworkList({}, {}, "").empty());
    }
}

TEST_SUITE_END();
