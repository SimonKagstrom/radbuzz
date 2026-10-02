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
    std::unique_ptr<ListenerCookie> AttachConnectionListener(std::function<void(bool connected)> cb)
    {
        // TODO: Actually implement
        return nullptr;
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

    void NotifyFromIsr() final
    {
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
    auto mailbox = post_office.Subscribe<MSG::incoming_call, MSG::call_ended>(notifier);

    ble.Start("ble");

    WHEN("an incoming call is received")
    {
        srv.Inject(kRxCharacteristicUuid,
                   "\x10GB({\"t\":\"call\",\"cmd\":\"incoming\",\"name\":\"Gregor "
                   "Samsa\",\"number\":\"+46701234567\"})\n");
        DoRunLoop();

        THEN("the caller is sent as a message")
        {
            auto msg = mailbox->Pop();
            REQUIRE(msg);
            REQUIRE(msg->Is<MSG::incoming_call>());
            CHECK(msg->As<MSG::incoming_call>()->caller_name == "Gregor Samsa");
            CHECK(msg->As<MSG::incoming_call>()->caller_number == "+46701234567");
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

            THEN("that is sent as a message")
            {
                bool ended = false;

                mailbox->Collect().On<MSG::call_ended>([&ended]() { ended = true; });
                REQUIRE(ended);
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

        THEN("no message is sent (for now)")
        {
            REQUIRE(mailbox->Pop() == std::nullopt);
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
