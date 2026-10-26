#include "hal/mock/mock_wifi_client.hh"
#include "mock_filesystem.hh"
#include "test.hh"
#include "thread_fixture.hh"
#include "wifi_handler.hh"

namespace
{

using State = WifiHandler::State;
using Event = hal::IWifiClient::Event;

class NullNotifier : public IEventNotifier
{
public:
    void Notify() final
    {
    }

    bool NotifyFromIsr() final
    {
        return false;
    }
};

// Secured access points, all with the same signal strength
std::vector<hal::IWifiClient::AccessPoint>
InRange(std::initializer_list<const char*> ssids)
{
    std::vector<hal::IWifiClient::AccessPoint> access_points;

    for (auto ssid : ssids)
    {
        access_points.push_back({ssid, -60, true});
    }

    return access_points;
}

std::vector<std::string>
Ssids(const MSG::wifi_scan_result& result)
{
    std::vector<std::string> ssids;

    for (const auto& network : result.networks)
    {
        ssids.push_back(network.ssid);
    }

    return ssids;
}

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        mock_filesystem::Clear();

        {
            auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
            ps.GetWritableReference<AS::configuration>().wifi_ssid_data.networks = {
                {"Home", "home-password"}};
        }

        // Default behavior of the wifi client: Scans finish immediately with
        // networks_in_range (unless scans_complete is cleared), and connections succeed
        // (unless connect_succeeds is cleared, or the SSID is in failing_ssids/hanging_ssids)
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, AttachListener(_))
                                     .LR_SIDE_EFFECT(m_on_wifi_event = _1)
                                     .RETURN(ListenerCookie([]() {})));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, Enable()).LR_SIDE_EFFECT(enable_count++));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, Disable()).LR_SIDE_EFFECT(disable_count++));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, Disconnect()).LR_SIDE_EFFECT(disconnect_count++));
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, StartScan()).LR_SIDE_EFFECT({
            scan_count++;
            if (scans_complete)
            {
                SendWifiEvent(Event::kScanDone);
            }
        }));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, GetScanResult()).LR_RETURN(networks_in_range));
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, Connect(_, _)).LR_SIDE_EFFECT({
            connect_count++;
            connected_ssid = _1;
            connected_password = _2;
            connect_attempts.push_back(_1);

            if (std::ranges::find(hanging_ssids, connected_ssid) != hanging_ssids.end())
            {
                return;
            }
            const auto fails = std::ranges::find(failing_ssids, connected_ssid) !=
                               failing_ssids.end();
            SendWifiEvent(connect_succeeds && !fails ? Event::kConnected : Event::kDisconnected);
        }));

        SetThread(&wifi_handler);
    }

    void Start()
    {
        wifi_handler.Start("wifi_handler");
        RunUntilStable();
    }

    // Run until the thread has nothing more to do (without advancing the time)
    void RunUntilStable()
    {
        for (auto i = 0; i < 100; i++)
        {
            if (!DoRunLoop())
            {
                return;
            }
        }

        FAIL("The wifi handler doesn't settle");
    }

    void Advance(milliseconds time)
    {
        AdvanceTimeAndRunLoop(time);
        RunUntilStable();
    }

    void SetMoving(bool moving)
    {
        state.Set<AS::is_moving>(moving);
        RunUntilStable();
    }

    void SendWifiEvent(Event event)
    {
        m_on_wifi_event(event);
    }

    State CurrentState() const
    {
        return wifi_handler.CurrentState();
    }

    // As from the UI
    void RequestScan()
    {
        post_office.Send<MSG::start_wifi_scan>();
        RunUntilStable();
    }

    void RequestConnect(const std::string& ssid, const std::string& password)
    {
        post_office.Send(MSG::wifi_connect {ssid, password});
        RunUntilStable();
    }

    std::vector<WifiSsidNetwork> KnownNetworks()
    {
        return state.Get<AS::configuration>()->wifi_ssid_data.networks;
    }

    // The scan results sent since the last call
    std::vector<MSG::wifi_scan_result> TakeScanResults()
    {
        std::vector<MSG::wifi_scan_result> results;

        m_mailbox->Collect().On<MSG::wifi_scan_result>(
            [&results](auto result) { results.push_back(*result); });

        return results;
    }

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;
    MockWifiClient wifi_client;
    Filesystem filesystem {"/"};

    // Wifi client behavior
    std::vector<hal::IWifiClient::AccessPoint> networks_in_range {InRange({"Neighbour", "Home"})};
    bool connect_succeeds {true};
    std::vector<std::string> failing_ssids;
    // Neither connected nor failed (until the test sends an event)
    std::vector<std::string> hanging_ssids;
    // Otherwise sent by the test
    bool scans_complete {true};

    // Wifi client calls
    unsigned enable_count {0};
    unsigned disable_count {0};
    unsigned scan_count {0};
    unsigned connect_count {0};
    unsigned disconnect_count {0};
    std::string connected_ssid;
    std::string connected_password;
    // The SSIDs of all Connect calls, in order
    std::vector<std::string> connect_attempts;

private:
    std::function<void(Event)> m_on_wifi_event;
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;

    NullNotifier m_notifier;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox {
        post_office.Subscribe<MSG::wifi_scan_result>(m_notifier)};

public:
    WifiHandler wifi_handler {state, post_office, filesystem, wifi_client};
};

} // namespace


TEST_SUITE_BEGIN("wifi_handler");

TEST_CASE_FIXTURE(Fixture, "a known network in range is connected at startup")
{
    Start();

    REQUIRE(enable_count == 1);
    REQUIRE(scan_count == 1);
    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(connected_ssid == "Home");
    REQUIRE(connected_password == "home-password");
    REQUIRE(state.Get<AS::wifi_connected>());
}

TEST_CASE_FIXTURE(Fixture, "networks from SSID.TXT are added to the known networks")
{
    mock_filesystem::SetFile("SSID.TXT", "Office\noffice-password\n");
    networks_in_range = InRange({"Office"});

    Start();

    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(connected_ssid == "Office");
    REQUIRE(connected_password == "office-password");
    // FNV-1a of the file. Changing the hash would import the file again on all boards
    REQUIRE(state.Get<AS::configuration>()->ssid_file_hash == 0x780438d9);
}

TEST_CASE_FIXTURE(Fixture, "SSID.TXT is parsed as SSID and password lines")
{
    auto require_imported = [this](const std::vector<WifiSsidNetwork>& from_file) {
        auto expected = std::vector<WifiSsidNetwork> {{"Home", "home-password"}};
        expected.insert(expected.end(), from_file.begin(), from_file.end());

        REQUIRE(state.Get<AS::configuration>()->wifi_ssid_data.networks == expected);
    };

    WHEN("the file has Windows line endings")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\r\noffice-password\r\nCafé\r\nlatte\r\n");
        Start();

        THEN("the \\r is not part of the SSID or password")
        {
            require_imported({{"Office", "office-password"}, {"Café", "latte"}});
        }
    }

    WHEN("a password line is empty")
    {
        mock_filesystem::SetFile("SSID.TXT", "Open cafe\n\nOffice\noffice-password\n");
        Start();

        THEN("it's an open network, and the rest of the file is also parsed")
        {
            require_imported({{"Open cafe", ""}, {"Office", "office-password"}});
        }
    }

    WHEN("the empty password line has a Windows line ending")
    {
        mock_filesystem::SetFile("SSID.TXT", "Open cafe\r\n\r\n");
        Start();

        THEN("it's still an open network")
        {
            require_imported({{"Open cafe", ""}});
        }
    }

    WHEN("the last line has no line ending")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\noffice-password");
        Start();

        THEN("the password is still read")
        {
            require_imported({{"Office", "office-password"}});
        }
    }

    WHEN("the password line is missing at the end of the file")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\noffice-password\nCafé\n");
        Start();

        THEN("the incomplete network is not added as an open network")
        {
            require_imported({{"Office", "office-password"}});
        }
    }

    WHEN("the file only has an SSID, without a line ending")
    {
        mock_filesystem::SetFile("SSID.TXT", "Café");
        Start();

        THEN("nothing is added")
        {
            require_imported({});
        }
    }

    WHEN("there is an empty line instead of an SSID")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\noffice-password\n\nCafé\nlatte\n");
        Start();

        THEN("parsing stops there")
        {
            require_imported({{"Office", "office-password"}});
        }
    }

    WHEN("a network in the file is already known")
    {
        mock_filesystem::SetFile("SSID.TXT", "Home\r\nnew-password\r\n");
        Start();

        THEN("only the password is updated")
        {
            REQUIRE(state.Get<AS::configuration>()->wifi_ssid_data.networks ==
                    std::vector<WifiSsidNetwork> {{"Home", "new-password"}});
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "SSID.TXT is only imported when it has changed")
{
    constexpr auto kOfficeFileHash = 0x780438d9u;

    // Imported on an earlier boot, and the Office network has since been forgotten
    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().ssid_file_hash = kOfficeFileHash;
    }
    networks_in_range = InRange({"Office"});

    WHEN("the file is unchanged")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\noffice-password\n");
        Start();

        THEN("the forgotten network is not added again")
        {
            const auto conf = state.Get<AS::configuration>();
            REQUIRE(conf->wifi_ssid_data == WifiSsidData {{{"Home", "home-password"}}});
            REQUIRE(conf->ssid_file_hash == kOfficeFileHash);
            REQUIRE(CurrentState() == State::kIdle);
        }
    }

    WHEN("the file has changed")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\nnew-password\n");
        Start();

        THEN("it's imported, and the new hash is stored")
        {
            const auto conf = state.Get<AS::configuration>();
            REQUIRE(conf->wifi_ssid_data ==
                    WifiSsidData {{{"Home", "home-password"}, {"Office", "new-password"}}});
            REQUIRE(conf->ssid_file_hash != kOfficeFileHash);
            REQUIRE(connected_ssid == "Office");
        }
    }

    WHEN("only the line endings have changed")
    {
        mock_filesystem::SetFile("SSID.TXT", "Office\r\noffice-password\r\n");
        Start();

        THEN("it counts as changed")
        {
            REQUIRE(state.Get<AS::configuration>()->ssid_file_hash != kOfficeFileHash);
        }
    }

    WHEN("the file is removed")
    {
        Start();

        THEN("the stored networks and hash are kept")
        {
            const auto conf = state.Get<AS::configuration>();
            REQUIRE(conf->wifi_ssid_data == WifiSsidData {{{"Home", "home-password"}}});
            REQUIRE(conf->ssid_file_hash == kOfficeFileHash);
        }
    }

    WHEN("the file is empty")
    {
        mock_filesystem::SetFile("SSID.TXT", "");
        Start();

        THEN("nothing is added, but the file is marked as imported")
        {
            const auto conf = state.Get<AS::configuration>();
            REQUIRE(conf->wifi_ssid_data == WifiSsidData {{{"Home", "home-password"}}});
            REQUIRE(conf->ssid_file_hash != kOfficeFileHash);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the wifi handler scans again when no known network is found")
{
    networks_in_range = InRange({"Neighbour"});

    Start();
    REQUIRE(CurrentState() == State::kIdle);
    REQUIRE(scan_count == 1);

    WHEN("10 seconds have passed")
    {
        Advance(10s);

        THEN("a new scan is done")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(CurrentState() == State::kIdle);
        }
    }

    WHEN("a known network comes in range")
    {
        networks_in_range = InRange({"Neighbour", "Home"});
        Advance(10s);

        THEN("it's connected")
        {
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "failed connections are retried before giving up on the network")
{
    connect_succeeds = false;

    Start();
    REQUIRE(CurrentState() == State::kConnecting);
    REQUIRE(connect_count == 1);

    WHEN("the retries fail")
    {
        Advance(5s);
        REQUIRE(connect_count == 2);
        Advance(5s);
        REQUIRE(connect_count == 3);
        Advance(5s);
        REQUIRE(connect_count == 4);
        REQUIRE(scan_count == 1);

        THEN("the wifi handler waits for a new scan")
        {
            REQUIRE(CurrentState() == State::kIdle);
        }

        AND_WHEN("the network is found again in the next scan")
        {
            Advance(10s);
            REQUIRE(scan_count == 2);

            THEN("it's not tried again")
            {
                REQUIRE(connect_count == 4);
                REQUIRE(CurrentState() == State::kIdle);
            }
        }
    }

    WHEN("a retry succeeds")
    {
        connect_succeeds = true;
        Advance(5s);

        THEN("the wifi is connected")
        {
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connect_count == 2);
            REQUIRE(scan_count == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the next known network is tried when one fails")
{
    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().wifi_ssid_data.networks = {
            {"Home", "home-password"}, {"Office", "office-password"}, {"Cafe", ""}};
    }
    networks_in_range = InRange({"Neighbour", "Cafe", "Office", "Home"});

    WHEN("the first one fails")
    {
        failing_ssids = {"Home"};
        Start();
        Advance(15s);

        THEN("the second one is connected, without a new scan")
        {
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Office");
            REQUIRE(connected_password == "office-password");
            REQUIRE(scan_count == 1);
        }
    }

    WHEN("all but the last fail")
    {
        failing_ssids = {"Home", "Office"};
        Start();
        Advance(15s);
        Advance(15s);

        THEN("the networks are tried in the configured order")
        {
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Cafe");
            REQUIRE(connect_attempts == std::vector<std::string> {"Home",
                                                                  "Home",
                                                                  "Home",
                                                                  "Home",
                                                                  "Office",
                                                                  "Office",
                                                                  "Office",
                                                                  "Office",
                                                                  "Cafe"});
            REQUIRE(scan_count == 1);
        }
    }

    WHEN("all fail")
    {
        failing_ssids = {"Home", "Office", "Cafe"};
        Start();
        Advance(15s);
        Advance(15s);
        Advance(15s);
        REQUIRE(CurrentState() == State::kIdle);
        const auto attempts = connect_count;

        AND_WHEN("a new known network comes in range")
        {
            {
                auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
                ps.GetWritableReference<AS::configuration>().wifi_ssid_data.networks.push_back(
                    {"Hotspot", "hotspot-password"});
            }
            networks_in_range = InRange({"Cafe", "Office", "Home", "Hotspot"});
            failing_ssids = {};
            Advance(10s);

            THEN("the configuration change makes all networks eligible again")
            {
                REQUIRE(CurrentState() == State::kConnected);
                REQUIRE(connected_ssid == "Home");
            }
        }

        AND_WHEN("only the scan changes")
        {
            networks_in_range = InRange({"Cafe", "Office", "Home", "Phone"});
            Advance(10s);

            THEN("the failed networks are not tried again")
            {
                REQUIRE(connect_count == attempts);
                REQUIRE(CurrentState() == State::kIdle);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "failed networks are tried again after moving")
{
    failing_ssids = {"Home"};
    Start();
    Advance(15s);
    REQUIRE(CurrentState() == State::kIdle);
    const auto attempts = connect_count;

    // The network works at the new place
    failing_ssids = {};

    WHEN("the moped moves and stops again")
    {
        SetMoving(true);
        Advance(10s);
        SetMoving(false);
        Advance(10s);

        THEN("the network is tried again")
        {
            REQUIRE(connect_count == attempts + 1);
            REQUIRE(CurrentState() == State::kConnected);
        }
    }

    WHEN("the moped stays")
    {
        Advance(1min);

        THEN("the network is not tried again")
        {
            REQUIRE(connect_count == attempts);
            REQUIRE(CurrentState() == State::kIdle);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a failed network is tried again when its password changes")
{
    failing_ssids = {"Home"};
    Start();
    Advance(15s);
    REQUIRE(CurrentState() == State::kIdle);

    failing_ssids = {};
    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().wifi_ssid_data.networks[0].password =
            "new-password";
    }
    Advance(10s);

    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(connected_password == "new-password");
}

TEST_CASE_FIXTURE(Fixture, "other configuration changes don't make a failed network eligible")
{
    failing_ssids = {"Home"};
    Start();
    Advance(15s);
    const auto attempts = connect_count;

    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().max_watts = 1234;
    }
    Advance(10s);

    REQUIRE(connect_count == attempts);
    REQUIRE(CurrentState() == State::kIdle);
}

TEST_CASE_FIXTURE(Fixture, "a lost connection is re-established when standing still")
{
    Start();
    Advance(2min);
    REQUIRE(CurrentState() == State::kConnected);

    WHEN("the connection is lost")
    {
        SendWifiEvent(Event::kDisconnected);
        RunUntilStable();

        THEN("the wifi handler scans and connects again")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(connect_count == 2);
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a lost connection shortly after stopping waits for a while before scanning")
{
    Start();
    SetMoving(true);
    Advance(10s);
    SetMoving(false);

    SendWifiEvent(Event::kDisconnected);
    RunUntilStable();
    REQUIRE(CurrentState() == State::kLostConnection);
    REQUIRE(state.Get<AS::wifi_connected>() == false);

    WHEN("the moped has been standing still for a while")
    {
        Advance(1min);

        THEN("the wifi handler scans and connects again")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the connection is kept while moving")
{
    Start();
    SetMoving(true);
    Advance(5min);

    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(disable_count == 0);
}

TEST_CASE_FIXTURE(Fixture, "the wifi is turned off when moving without a connection")
{
    Start();
    SetMoving(true);

    SendWifiEvent(Event::kDisconnected);
    RunUntilStable();
    REQUIRE(CurrentState() == State::kLostConnection);

    Advance(1min);
    REQUIRE(CurrentState() == State::kOff);
    REQUIRE(disable_count == 1);

    WHEN("the moped stops for a while")
    {
        SetMoving(false);
        Advance(1min);

        THEN("the wifi is turned on and connected again")
        {
            REQUIRE(enable_count == 2);
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the wifi is turned off when scanning while moving")
{
    networks_in_range = InRange({"Neighbour"});
    // Moving already at startup
    state.Set<AS::is_moving>(true);

    Start();
    REQUIRE(CurrentState() == State::kIdle);

    // Idle -> Scanning every 10 seconds, until moving for a minute
    Advance(1min);

    REQUIRE(CurrentState() == State::kOff);
}

TEST_CASE_FIXTURE(Fixture, "the wifi handler sends the result of every scan")
{
    networks_in_range = InRange({"Neighbour"});

    Start();

    auto results = TakeScanResults();
    REQUIRE(results.size() == 1);
    REQUIRE(Ssids(results[0]) == std::vector<std::string> {"Neighbour"});

    WHEN("the periodic scan finds a known network")
    {
        networks_in_range = InRange({"Neighbour", "Home"});
        Advance(10s);

        THEN("that result is also sent")
        {
            results = TakeScanResults();
            REQUIRE(results.size() == 1);
            REQUIRE(Ssids(results[0]) == std::vector<std::string> {"Neighbour", "Home"});
            REQUIRE(CurrentState() == State::kConnected);
        }

        AND_WHEN("time passes while connected")
        {
            TakeScanResults();
            Advance(5min);

            THEN("no more scans, and no more results, are made")
            {
                REQUIRE(TakeScanResults().empty());
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the scan result has unique, non-empty SSIDs in the scanned order")
{
    // Hidden networks, and the same network from several access points
    networks_in_range = InRange({"Neighbour", "", "Home", "Neighbour", "", "Café 5G", "Home"});

    Start();

    auto results = TakeScanResults();
    REQUIRE(results.size() == 1);
    REQUIRE(Ssids(results[0]) == std::vector<std::string> {"Neighbour", "Home", "Café 5G"});
}

TEST_CASE_FIXTURE(Fixture, "the scan result has the signal strength and type of the networks")
{
    networks_in_range = {
        {"Home", -42, true},
        {"Café", -67, false},
        // Extremes of int8_t
        {"Far away", -128, true},
        {"Next to it", 127, false},
    };

    Start();

    auto results = TakeScanResults();
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].networks == std::vector<MSG::WifiScanEntry> {
                                       {"Home", -42, MSG::WifiType::kSecured},
                                       {"Café", -67, MSG::WifiType::kOpen},
                                       {"Far away", -128, MSG::WifiType::kSecured},
                                       {"Next to it", 127, MSG::WifiType::kOpen},
                                   });
}

TEST_CASE_FIXTURE(Fixture, "the first of several access points with the same SSID is kept")
{
    // The ESP32 driver sorts by RSSI, so the first is the strongest
    networks_in_range = {
        {"Mesh", -50, true},
        {"Neighbour", -60, true},
        {"Mesh", -80, true},
        // Same SSID, but a different type (e.g., a guest network). Still the same name to pick
        {"Mesh", -55, false},
    };

    Start();

    auto results = TakeScanResults();
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].networks == std::vector<MSG::WifiScanEntry> {
                                       {"Mesh", -50, MSG::WifiType::kSecured},
                                       {"Neighbour", -60, MSG::WifiType::kSecured},
                                   });
}

TEST_CASE_FIXTURE(Fixture, "an empty scan is also sent")
{
    auto require_empty_result = [this]() {
        auto results = TakeScanResults();
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].networks.empty());
        REQUIRE(CurrentState() == State::kIdle);
    };

    WHEN("nothing is in range")
    {
        networks_in_range = {};
        Start();

        THEN("an empty result is sent")
        {
            require_empty_result();
        }
    }

    WHEN("only hidden networks are in range")
    {
        networks_in_range = InRange({"", ""});
        Start();

        THEN("an empty result is sent")
        {
            require_empty_result();
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a requested scan is done directly")
{
    WHEN("idle")
    {
        networks_in_range = InRange({"Neighbour"});
        Start();
        REQUIRE(CurrentState() == State::kIdle);
        TakeScanResults();

        RequestScan();

        THEN("there's no wait for the next periodic scan")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(TakeScanResults().size() == 1);
            REQUIRE(CurrentState() == State::kIdle);
        }
    }

    WHEN("connected")
    {
        Start();
        REQUIRE(CurrentState() == State::kConnected);
        TakeScanResults();

        RequestScan();

        THEN("the scan is done without disconnecting")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(TakeScanResults().size() == 1);
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(disconnect_count == 0);
            REQUIRE(connect_count == 1);
            REQUIRE(state.Get<AS::wifi_connected>());
        }
    }

    WHEN("the connection has just been lost")
    {
        Start();
        SendWifiEvent(Event::kDisconnected);
        SetMoving(true);
        RunUntilStable();
        REQUIRE(CurrentState() == State::kLostConnection);
        networks_in_range = InRange({"Neighbour"});

        RequestScan();

        THEN("there's no wait for standing still")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(CurrentState() == State::kIdle);
        }
    }

    WHEN("the wifi is off since moving")
    {
        Start();
        SetMoving(true);
        SendWifiEvent(Event::kDisconnected);
        Advance(1min);
        REQUIRE(CurrentState() == State::kOff);
        TakeScanResults();

        RequestScan();

        THEN("the wifi is turned on, and the scan finishes although still moving")
        {
            REQUIRE(enable_count == 2);
            REQUIRE(scan_count == 2);
            REQUIRE(TakeScanResults().size() == 1);
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a scan requested while connecting is done afterwards")
{
    connect_succeeds = false;
    Start();
    REQUIRE(CurrentState() == State::kConnecting);

    RequestScan();
    REQUIRE(scan_count == 1);

    WHEN("the connection fails")
    {
        Advance(15s);

        THEN("the scan is done directly, without the idle time")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(CurrentState() == State::kIdle);
        }
    }

    WHEN("the connection succeeds")
    {
        connect_succeeds = true;
        Advance(5s);

        THEN("the scan is done while connected")
        {
            REQUIRE(scan_count == 2);
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "requests during a scan are handled by that scan")
{
    scans_complete = false;
    networks_in_range = InRange({"Neighbour"});
    Start();
    REQUIRE(CurrentState() == State::kScanning);

    RequestScan();
    RequestConnect("Neighbour", "neighbour-password");
    REQUIRE(scan_count == 1);

    SendWifiEvent(Event::kScanDone);
    RunUntilStable();

    REQUIRE(scan_count == 1);
    REQUIRE(TakeScanResults().size() == 1);
    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(connected_ssid == "Neighbour");
}

TEST_CASE_FIXTURE(Fixture, "a requested network is stored first, and connected to")
{
    WHEN("it's a new network")
    {
        networks_in_range = InRange({"Neighbour"});
        Start();
        REQUIRE(CurrentState() == State::kIdle);

        RequestConnect("Neighbour", "neighbour-password");

        THEN("it's stored and connected")
        {
            REQUIRE(KnownNetworks() == std::vector<WifiSsidNetwork> {
                                           {"Neighbour", "neighbour-password"},
                                           {"Home", "home-password"},
                                       });
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Neighbour");
            REQUIRE(connected_password == "neighbour-password");
            REQUIRE(scan_count == 2);
        }
    }

    WHEN("it's already known")
    {
        {
            auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
            ps.GetWritableReference<AS::configuration>().wifi_ssid_data.networks.push_back(
                {"Office", "old-password"});
        }
        networks_in_range = InRange({"Office"});
        Start();

        RequestConnect("Office", "new-password");

        THEN("it's moved first, with the new password")
        {
            REQUIRE(KnownNetworks() == std::vector<WifiSsidNetwork> {
                                           {"Office", "new-password"},
                                           {"Home", "home-password"},
                                       });
            REQUIRE(connected_password == "new-password");
        }
    }

    WHEN("it's an open network")
    {
        networks_in_range = {{"Cafe", -60, false}};
        Start();

        RequestConnect("Cafe", "");

        THEN("it's connected without a password")
        {
            REQUIRE(KnownNetworks().front() == WifiSsidNetwork {"Cafe", ""});
            REQUIRE(connected_ssid == "Cafe");
            REQUIRE(connected_password == "");
        }
    }

    WHEN("it's not in range")
    {
        networks_in_range = InRange({"Neighbour"});
        Start();

        RequestConnect("Office", "office-password");

        THEN("it's still stored")
        {
            REQUIRE(KnownNetworks().front() == WifiSsidNetwork {"Office", "office-password"});
            REQUIRE(CurrentState() == State::kIdle);
        }

        AND_WHEN("it comes in range")
        {
            networks_in_range = InRange({"Neighbour", "Office"});
            Advance(10s);

            THEN("it's connected")
            {
                REQUIRE(CurrentState() == State::kConnected);
                REQUIRE(connected_ssid == "Office");
            }
        }
    }

    WHEN("the wifi is off since moving")
    {
        Start();
        SetMoving(true);
        SendWifiEvent(Event::kDisconnected);
        Advance(1min);
        REQUIRE(CurrentState() == State::kOff);
        networks_in_range = InRange({"Home", "Office"});

        RequestConnect("Office", "office-password");

        THEN("the wifi is turned on and connected")
        {
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Office");
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a requested network replaces the current connection")
{
    networks_in_range = InRange({"Home", "Office"});

    WHEN("connected")
    {
        Start();
        REQUIRE(connected_ssid == "Home");

        RequestConnect("Office", "office-password");

        THEN("the current network is disconnected first")
        {
            REQUIRE(disconnect_count == 1);
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Office");
            REQUIRE(state.Get<AS::wifi_connected>());
        }
    }

    WHEN("waiting to retry a failed connection")
    {
        failing_ssids = {"Home"};
        Start();
        REQUIRE(CurrentState() == State::kConnecting);

        RequestConnect("Office", "office-password");

        THEN("the retries are abandoned")
        {
            REQUIRE(disconnect_count == 0);
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Office");
        }
    }

    WHEN("a connection attempt is ongoing")
    {
        hanging_ssids = {"Home"};
        Start();
        REQUIRE(CurrentState() == State::kConnecting);

        RequestConnect("Office", "office-password");

        THEN("the attempt is stopped")
        {
            REQUIRE(disconnect_count == 1);
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Office");
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a requested network with a wrong password is kept")
{
    networks_in_range = InRange({"Home", "Office"});
    failing_ssids = {"Office"};
    Start();

    RequestConnect("Office", "wrong-password");
    Advance(15s);

    THEN("the next known network is used, but the requested one is still stored")
    {
        REQUIRE(CurrentState() == State::kConnected);
        REQUIRE(connected_ssid == "Home");
        REQUIRE(KnownNetworks().front() == WifiSsidNetwork {"Office", "wrong-password"});
    }

    AND_WHEN("it's requested again, with the right password")
    {
        failing_ssids = {};
        RequestConnect("Office", "right-password");

        THEN("it's tried again, although it failed here")
        {
            REQUIRE(CurrentState() == State::kConnected);
            REQUIRE(connected_ssid == "Office");
            REQUIRE(connected_password == "right-password");
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a failed network requested again during a scan is tried")
{
    networks_in_range = InRange({"Office"});
    failing_ssids = {"Office"};
    Start();
    RequestConnect("Office", "wrong-password");
    Advance(15s);
    REQUIRE(CurrentState() == State::kIdle);

    // The periodic scan is ongoing when the password is retyped
    failing_ssids = {};
    scans_complete = false;
    Advance(10s);
    REQUIRE(CurrentState() == State::kScanning);

    // Handled in the same activation, i.e., before the configuration change is seen
    post_office.Send(MSG::wifi_connect {"Office", "right-password"});
    SendWifiEvent(Event::kScanDone);
    RunUntilStable();

    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(connected_password == "right-password");
}

TEST_CASE_FIXTURE(Fixture, "invalid requested networks are ignored")
{
    Start();
    REQUIRE(CurrentState() == State::kConnected);
    const auto known = KnownNetworks();

    WHEN("the SSID is empty")
    {
        RequestConnect("", "password");
    }
    WHEN("the SSID is too long")
    {
        RequestConnect(std::string(33, 'x'), "password");
    }
    WHEN("the password is too long")
    {
        RequestConnect("Office", std::string(65, 'x'));
    }

    REQUIRE(KnownNetworks() == known);
    // Not disconnected for nothing
    REQUIRE(disconnect_count == 0);
    REQUIRE(scan_count == 1);
    REQUIRE(CurrentState() == State::kConnected);
}

TEST_CASE_FIXTURE(Fixture, "requested networks at the length limits are accepted")
{
    const auto ssid = std::string(32, 's');
    const auto password = std::string(64, 'a');
    networks_in_range = InRange({ssid.c_str()});
    Start();

    RequestConnect(ssid, password);

    REQUIRE(KnownNetworks().front() == WifiSsidNetwork {ssid, password});
    REQUIRE(connected_ssid == ssid);
}

TEST_SUITE_END();
