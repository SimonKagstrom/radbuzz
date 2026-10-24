#include "ble_handler.hh"
#include "line_collector.hh"
#include "test.hh"
#include "thread_fixture.hh"

#include <map>

namespace
{

class BleServerStub : public hal::IBleServer, public hal::IBleClient
{
public:
    void Inject(auto uuid, const std::string& data)
    {
        Inject(uuid, {reinterpret_cast<const uint8_t*>(data.data()), data.size()});
    }

    void Inject(auto uuid, std::span<const uint8_t> data)
    {
        auto uuid128 = hal::detail::StringToUuid128(uuid);
        REQUIRE(m_uuid_cb.find(uuid128[0]) != m_uuid_cb.end());

        m_uuid_cb[uuid128[0]](data);
    }

private:
    ListenerCookie AttachConnectionListener(std::function<void(bool connected)> cb)
    {
        // TODO: Actually implement
        return {};
    }


    void SetServiceUuid128(std::span<const uint8_t, 16> service_uuid) final
    {
    }

    void AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final
    {
        // For now assume they are unique
        REQUIRE(m_uuid_cb.find(uuid[0]) == m_uuid_cb.end());
        m_uuid_cb[uuid[0]] = data;
    }

    void AddNotifyGattCharacteristics(std::span<const uint8_t, 16> uuid) final
    {
    }

    bool Notify(std::span<const uint8_t, 16> uuid, std::span<const uint8_t> data) final
    {
        notified[uuid[0]] += std::string(reinterpret_cast<const char*>(data.data()), data.size());
        return true;
    }

    void Start() final
    {
    }

    void PollEvents() final
    {
    }

    void
    ScanForService(hal::Uuid128Span service_uuid,
                   const ScanFilter& filter,
                   const std::function<void(std::unique_ptr<hal::IBleClient::IPeer>)>& cb) final
    {
        // Not relevant for now
    }

    std::map<uint8_t, std::function<void(std::span<const uint8_t>)>> m_uuid_cb;

public:
    std::map<uint8_t, std::string> notified;
};


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

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(&ble);
    }

    BleServerStub srv;
    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;
    ImageCache cache;

    BleHandler ble {srv, srv, state, post_office, cache};
};

} // namespace

TEST_SUITE_BEGIN("ble_handler");

TEST_CASE("the line collector splits data into lines")
{
    LineCollector collector;

    WHEN("a line is split over several chunks")
    {
        collector.Push("GB({\"t\":");
        REQUIRE(collector.Poll() == std::nullopt);
        collector.Push("\"notify\"})\n");

        THEN("it's returned once complete")
        {
            REQUIRE(collector.Poll() == "GB({\"t\":\"notify\"})");
            REQUIRE(collector.Poll() == std::nullopt);
        }
    }

    WHEN("a chunk contains the end of one line and the start of another")
    {
        collector.Push("first\nsec");
        collector.Push("ond\n");

        THEN("both lines are returned")
        {
            REQUIRE(collector.Poll() == "first");
            REQUIRE(collector.Poll() == "second");
            REQUIRE(collector.Poll() == std::nullopt);
        }
    }
}

TEST_CASE("gadgetbridge lines are parsed into json")
{
    WHEN("a GB message arrives")
    {
        auto json = GadgetBridgeProtocol::ParseLine(
            "\x10GB({\"t\":\"notify\",\"id\":1,\"body\":\"Hall\\xe5\"})");

        THEN("it's parsed, including JavaScript escapes")
        {
            REQUIRE(json);
            CHECK((*json)["t"] == "notify");
            CHECK((*json)["id"] == 1);
            CHECK((*json)["body"] == "Hall\u00e5");
        }
    }

    WHEN("Olsmässgatan arrives")
    {
        auto json = GadgetBridgeProtocol::ParseLine("\x10GB({\"t\":\"nav\",\"instr\":\"towards "
                                                    "Olsm\xe4ssgatan\",\"distance\":\"0\xa0m\","
                                                    "\"action\":\"continue\",\"eta\":\"13:38\"})");
        THEN("it's parsed correctly")
        {
            REQUIRE(json);
            CHECK((*json)["t"] == "nav");
            CHECK((*json)["instr"] == "towards Olsmässgatan");
            // Non-breaking space
            CHECK((*json)["distance"] == "0 m");
            CHECK((*json)["action"] == "continue");
            CHECK((*json)["eta"] == "13:38");
        }
    }

    WHEN("other JavaScript arrives")
    {
        THEN("it's ignored")
        {
            REQUIRE(GadgetBridgeProtocol::ParseLine("\x10setTime(1234);E.setTimeZone(2.0);") ==
                    std::nullopt);
            REQUIRE(GadgetBridgeProtocol::ParseLine("GB({broken)") == std::nullopt);
        }
    }
}

TEST_CASE("gadgetbridge distances are converted to meters")
{
    WHEN("metric distances arrive")
    {
        THEN("they are converted")
        {
            CHECK(GadgetBridgeProtocol::DistanceToMeters("300 m") == 300);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("0 m") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("1.4 km") == 1400);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("1,4 km") == 1400);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("12 km") == 12000);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("50m") == 50);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("2.5 KM") == 2500);
            CHECK(GadgetBridgeProtocol::DistanceToMeters(".5 km") == 500);
        }
    }

    WHEN("imperial distances arrive")
    {
        THEN("they are converted and rounded")
        {
            CHECK(GadgetBridgeProtocol::DistanceToMeters("500 ft") == 152);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("100 yd") == 91);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("0.2 mi") == 322);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("1 mi") == 1609);
        }
    }

    WHEN("other spaces are used")
    {
        THEN("they are accepted")
        {
            // No-break and narrow no-break space (UTF-8, after ParseLine)
            CHECK(GadgetBridgeProtocol::DistanceToMeters("300\xc2\xa0m") == 300);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("1.4\xe2\x80\xafkm") == 1400);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("  300  m  ") == 300);
        }
    }

    WHEN("a plain number arrives")
    {
        THEN("it's meters")
        {
            CHECK(GadgetBridgeProtocol::DistanceToMeters("75") == 75);
        }
    }

    WHEN("a full nav line arrives")
    {
        auto json = GadgetBridgeProtocol::ParseLine(
            "GB({\"t\":\"nav\",\"distance\":\"1,2\xa0km\",\"action\":\"left\"})");

        THEN("the distance is converted after parsing")
        {
            REQUIRE(json);
            CHECK(GadgetBridgeProtocol::DistanceToMeters((*json)["distance"].get<std::string>()) ==
                  1200);
        }
    }

    WHEN("broken input arrives")
    {
        THEN("0 is returned")
        {
            CHECK(GadgetBridgeProtocol::DistanceToMeters("") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters(" ") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("m") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("km 300") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("-300 m") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("300 parsecs") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("300 mm") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("1.2.3 km") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("300 m extra") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters("99999999999999999999 m") == 0);
            CHECK(GadgetBridgeProtocol::DistanceToMeters(std::string_view("3\0 m", 4)) == 0);
        }
    }
}

TEST_CASE("gadgetbridge distances are rounded to avoid flicker")
{
    WHEN("the distance is below 100 m")
    {
        THEN("it's rounded to 5 m")
        {
            CHECK(GadgetBridgeProtocol::RoundDistance(12) == 10);
            CHECK(GadgetBridgeProtocol::RoundDistance(13) == 15);
            CHECK(GadgetBridgeProtocol::RoundDistance(15) == 15);
            CHECK(GadgetBridgeProtocol::RoundDistance(97) == 95);
            CHECK(GadgetBridgeProtocol::RoundDistance(98) == 100);
        }
    }

    WHEN("the distance is 100 m or more")
    {
        THEN("it's rounded to 10 m")
        {
            CHECK(GadgetBridgeProtocol::RoundDistance(100) == 100);
            CHECK(GadgetBridgeProtocol::RoundDistance(104) == 100);
            CHECK(GadgetBridgeProtocol::RoundDistance(105) == 110);
            CHECK(GadgetBridgeProtocol::RoundDistance(152) == 150);
            CHECK(GadgetBridgeProtocol::RoundDistance(1609) == 1610);
        }
    }

    WHEN("the distance is very short")
    {
        THEN("it's not rounded to 0, which means no navigation")
        {
            CHECK(GadgetBridgeProtocol::RoundDistance(0) == 0);
            CHECK(GadgetBridgeProtocol::RoundDistance(1) == 5);
            CHECK(GadgetBridgeProtocol::RoundDistance(2) == 5);
            CHECK(GadgetBridgeProtocol::RoundDistance(3) == 5);
        }
    }

    WHEN("the distance is huge")
    {
        THEN("it doesn't overflow")
        {
            CHECK(GadgetBridgeProtocol::RoundDistance(std::numeric_limits<uint32_t>::max()) ==
                  4294967290);
            CHECK(GadgetBridgeProtocol::RoundDistance(4294967284) == 4294967280);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler accepts data on the UART characteristic")
{
    ble.Start("ble");

    srv.Inject(kRxCharacteristicUuid, "\x10GB({\"t\":\"notify\"})\n");
    DoRunLoop();
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler replies to GPS status requests")
{
    ble.Start("ble");

    srv.Inject(kRxCharacteristicUuid, "\x10GB({\"t\":\"is_gps_active\"})\n");
    DoRunLoop();

    auto tx = hal::detail::StringToUuid128(kTxCharacteristicUuid);
    REQUIRE(srv.notified[tx[0]] == "{\"status\":false,\"t\":\"gps_power\"}\r\n");
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler forwards call events from Gadgetbridge")
{
    NullNotifier notifier;
    auto mailbox =
        post_office.Subscribe<MSG::incoming_call, MSG::call_ended, MSG::call_started>(notifier);

    std::vector<MSG::incoming_call> calls;
    auto ended = 0;
    auto started = 0;
    auto collect = [&]() {
        mailbox->Collect()
            .On<MSG::incoming_call>([&calls](auto call) { calls.push_back(*call); })
            .On<MSG::call_ended>([&ended]() { ended++; })
            .On<MSG::call_started>([&started]() { started++; });
    };

    ble.Start("ble");

    WHEN("an incoming call is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"call\",\"cmd\":\"incoming\",\"name\":\"Gregor "
                   "Samsa\",\"number\":\"+46701234567\"})\n");
        DoRunLoop();

        THEN("the caller is sent as a message")
        {
            collect();
            REQUIRE(calls.size() == 1);
            CHECK(calls[0].caller_name == "Gregor Samsa");
            CHECK(calls[0].caller_number == "+46701234567");
            CHECK(ended == 0);
        }
        AND_THEN("the call ongoing status is updated")
        {
            CHECK(state.Get<AS::call_ongoing>() == true);
        }

        AND_WHEN("the call ends")
        {
            srv.Inject(kRxCharacteristicUuid,
                       "\x10GB({\"t\":\"call\",\"cmd\":\"end\",\"name\":\"\",\"number\":\"\"})\n");
            DoRunLoop();

            THEN("that is sent as a message, after the incoming call")
            {
                collect();
                CHECK(calls.size() == 1);
                CHECK(ended == 1);
            }
            AND_THEN("the call ongoing status is updated")
            {
                CHECK(state.Get<AS::call_ongoing>() == false);
            }
        }
    }

    WHEN("the call is answered")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"call\",\"cmd\":\"start\",\"name\":\"\",\"number\":\"\"})\n");
        DoRunLoop();

        THEN("that is sent as a message")
        {
            collect();
            CHECK(started == 1);
            CHECK(calls.empty());
            CHECK(ended == 0);
        }
        AND_THEN("the call is ongoing")
        {
            CHECK(state.Get<AS::call_ongoing>() == true);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler forwards navigation from Gadgetbridge")
{
    ble.Start("ble");

    WHEN("a navigation instruction is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"nav\",\"action\":\"right\",\"distance\":\"160 m\","
                   "\"eta\":\"06:46\",\"instr\":\"Brittm\xe4ssgatan\"})\n");
        DoRunLoop();

        THEN("the navigation state is updated")
        {
            CHECK(state.Get<AS::navigation_active>() == true);
            CHECK(*state.Get<AS::next_street>() == "Brittmässgatan");
            CHECK(state.Get<AS::distance_to_next>() == 160);
            CHECK(state.Get<AS::turn_symbol>() == TurnSymbol::kTurnRight);
        }

        AND_WHEN("navigation ends")
        {
            srv.Inject(kRxCharacteristicUuid, "\x10GB({\"t\":\"nav\"})\n");
            DoRunLoop();

            THEN("navigation is no longer active")
            {
                CHECK(state.Get<AS::navigation_active>() == false);
            }
        }
    }

    WHEN("a navigation instruction with broken field types is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"nav\",\"action\":7,\"distance\":null,\"eta\":[],"
                   "\"instr\":{\"a\":1}})\n");
        DoRunLoop();

        THEN("the broken fields are treated as empty")
        {
            CHECK(state.Get<AS::navigation_active>() == true);
            CHECK(*state.Get<AS::next_street>() == "");
            CHECK(state.Get<AS::distance_to_next>() == 0);
            CHECK(state.Get<AS::turn_symbol>() == TurnSymbol::kNone);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler forwards notifications from Gadgetbridge")
{
    NullNotifier notifier;
    auto mailbox = post_office.Subscribe<MSG::message, MSG::dismiss_message>(notifier);

    std::vector<MSG::message> messages;
    std::vector<MSG::dismiss_message> dismissed;
    auto collect = [&]() {
        mailbox->Collect()
            .On<MSG::message>([&messages](auto msg) { messages.push_back(*msg); })
            .On<MSG::dismiss_message>([&dismissed](auto msg) { dismissed.push_back(*msg); });
    };

    ble.Start("ble");

    WHEN("a notification is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"notify\",\"id\":1790422616,\"src\":\"Gmail\",\"title\":"
                   "\"Prisjakt\",\"subject\":\"\",\"sender\":\"\",\"body\":\"Logga in\\nDags\","
                   "\"reply\":true,\"actions\":[{\"title\":\"Archive\"}]})\n");
        DoRunLoop();

        THEN("it is sent as a message")
        {
            collect();
            REQUIRE(messages.size() == 1);
            CHECK(messages[0].id == 1790422616);
            CHECK(messages[0].source == "Gmail");
            CHECK(messages[0].title == "Prisjakt");
            CHECK(messages[0].body == "Logga in\nDags");
            CHECK(dismissed.empty());
        }
    }

    WHEN("a notification without a title is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"notify\",\"id\":-2,\"src\":\"SMS Message\",\"title\":\"\","
                   "\"subject\":\"Hello\",\"sender\":\"+46701234567\",\"body\":\"Hi\"})\n");
        DoRunLoop();

        THEN("the sender is the title, and the subject goes first in the body")
        {
            collect();
            REQUIRE(messages.size() == 1);
            CHECK(messages[0].id == static_cast<uint32_t>(-2));
            CHECK(messages[0].title == "+46701234567");
            CHECK(messages[0].body == "Hello\nHi");
        }
    }

    WHEN("a notification without an id is received")
    {
        srv.Inject(kRxCharacteristicUuid, "\x10GB({\"t\":\"notify\",\"body\":\"Hi\"})\n");
        DoRunLoop();

        THEN("it is ignored")
        {
            collect();
            CHECK(messages.empty());
            CHECK(dismissed.empty());
        }
    }

    WHEN("a notification with a broken id is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"notify\",\"id\":\"Simon\",\"body\":\"Hi\"})\n");
        srv.Inject(kRxCharacteristicUuid, "\x10GB({\"t\":\"notify-\",\"id\":\"Simon\"})\n");
        DoRunLoop();

        THEN("they are ignored")
        {
            collect();
            CHECK(messages.empty());
            CHECK(dismissed.empty());
        }
    }

    WHEN("a notification is dismissed on the phone")
    {
        srv.Inject(kRxCharacteristicUuid, "\x10GB({\"t\":\"notify-\",\"id\":1790422616})\n");
        DoRunLoop();

        THEN("that is sent as a message")
        {
            collect();
            REQUIRE(dismissed.size() == 1);
            CHECK(dismissed[0].id == 1790422616);
            CHECK(messages.empty());
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler sends message dismissals to Gadgetbridge")
{
    ble.Start("ble");
    auto tx = hal::detail::StringToUuid128(kTxCharacteristicUuid);

    WHEN("a message is dismissed by the user")
    {
        post_office.Send<MSG::user_dismissed_message>({1790422616});
        DoRunLoop();

        THEN("it's dismissed on the phone as well")
        {
            REQUIRE(srv.notified[tx[0]] ==
                    "{\"id\":1790422616,\"n\":\"DISMISS\",\"t\":\"notify\"}\r\n");
        }
    }

    WHEN("a message with a negative (Java int) id is dismissed")
    {
        post_office.Send<MSG::user_dismissed_message>({static_cast<uint32_t>(-2)});
        DoRunLoop();

        THEN("the id is sent back as negative")
        {
            REQUIRE(srv.notified[tx[0]] == "{\"id\":-2,\"n\":\"DISMISS\",\"t\":\"notify\"}\r\n");
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the BLE handler sends call control to Gadgetbridge")
{
    ble.Start("ble");
    auto tx = hal::detail::StringToUuid128(kTxCharacteristicUuid);

    WHEN("the call is answered")
    {
        post_office.Send<MSG::answer_call>();
        DoRunLoop();

        THEN("accept is sent")
        {
            REQUIRE(srv.notified[tx[0]] == "{\"n\":\"accept\",\"t\":\"call\"}\r\n");
        }
    }

    WHEN("the call is declined")
    {
        post_office.Send<MSG::decline_call>();
        DoRunLoop();

        THEN("reject is sent")
        {
            REQUIRE(srv.notified[tx[0]] == "{\"n\":\"reject\",\"t\":\"call\"}\r\n");
        }
    }

    WHEN("the call is hung up")
    {
        post_office.Send<MSG::hangup_call>();
        DoRunLoop();

        THEN("end is sent")
        {
            REQUIRE(srv.notified[tx[0]] == "{\"n\":\"end\",\"t\":\"call\"}\r\n");
        }
    }
}

TEST_SUITE_END();
