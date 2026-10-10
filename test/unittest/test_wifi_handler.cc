#include "hal/mock/mock_wifi_client.hh"
#include "mock_filesystem.hh"
#include "test.hh"
#include "thread_fixture.hh"
#include "wifi_handler.hh"

namespace
{

using State = WifiHandler::State;
using Event = hal::IWifiClient::Event;

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
        // networks_in_range, and connections succeed (unless connect_succeeds is cleared)
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, AttachListener(_))
                                     .LR_SIDE_EFFECT(m_on_wifi_event = _1)
                                     .RETURN(std::make_unique<ListenerCookie>([]() {})));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, Enable()).LR_SIDE_EFFECT(enable_count++));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, Disable()).LR_SIDE_EFFECT(disable_count++));
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, Disconnect()));
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, StartScan()).LR_SIDE_EFFECT({
            scan_count++;
            SendWifiEvent(Event::kScanDone);
        }));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(wifi_client, GetScanResult()).LR_RETURN(networks_in_range));
        m_expectations.push_back(NAMED_ALLOW_CALL(wifi_client, Connect(_, _)).LR_SIDE_EFFECT({
            connect_count++;
            connected_ssid = _1;
            connected_password = _2;
            SendWifiEvent(connect_succeeds ? Event::kConnected : Event::kDisconnected);
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
        state.CheckoutReadWrite().Set<AS::is_moving>(moving);
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

    ApplicationState state;
    MockWifiClient wifi_client;
    Filesystem filesystem {"/"};

    // Wifi client behavior
    std::vector<std::string> networks_in_range {"Neighbour", "Home"};
    bool connect_succeeds {true};

    // Wifi client calls
    unsigned enable_count {0};
    unsigned disable_count {0};
    unsigned scan_count {0};
    unsigned connect_count {0};
    std::string connected_ssid;
    std::string connected_password;

private:
    std::function<void(Event)> m_on_wifi_event;
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;

public:
    WifiHandler wifi_handler {state, filesystem, wifi_client};
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
    networks_in_range = {"Office"};

    Start();

    REQUIRE(CurrentState() == State::kConnected);
    REQUIRE(connected_ssid == "Office");
    REQUIRE(connected_password == "office-password");
}

TEST_CASE_FIXTURE(Fixture, "the wifi handler scans again when no known network is found")
{
    networks_in_range = {"Neighbour"};

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
        networks_in_range = {"Neighbour", "Home"};
        Advance(10s);

        THEN("it's connected")
        {
            REQUIRE(CurrentState() == State::kConnected);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "failed connections are retried before scanning again")
{
    connect_succeeds = false;

    Start();
    REQUIRE(CurrentState() == State::kRetryConnect);
    REQUIRE(connect_count == 1);

    WHEN("the retries fail")
    {
        Advance(5s);
        REQUIRE(connect_count == 2);
        Advance(5s);
        REQUIRE(connect_count == 3);
        REQUIRE(scan_count == 1);

        Advance(5s);

        THEN("a new scan is done after the last retry")
        {
            REQUIRE(scan_count == 2);
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
    networks_in_range = {"Neighbour"};
    // Moving already at startup
    state.CheckoutReadWrite().Set<AS::is_moving>(true);

    Start();
    REQUIRE(CurrentState() == State::kIdle);

    // Idle -> Scanning every 10 seconds, until moving for a minute
    Advance(1min);

    REQUIRE(CurrentState() == State::kOff);
}

TEST_SUITE_END();
