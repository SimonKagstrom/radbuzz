#include "mock_filesystem.hh"
#include "mock_https_client.hh"
#include "test.hh"
#include "thread_fixture.hh"
#include "traccar_uploader.hh"

#include <format>
#include <string>
#include <vector>

namespace
{

constexpr uint32_t kUtc = 1774269319;
constexpr auto kServer = "https://pi.tail.ts.net";
constexpr auto kConfiguration =
    R"({"url": "https://pi.tail.ts.net/", "id": "radbuzz", "demo_id": "radbuzz-demo"})";
constexpr auto kLogPath = "trips/log/69c13387.jsl";
constexpr auto kSentPath = "trips/sent/69c13387.jsl";

// A line as written by the TripRecorder, with 36 km/h
std::string
Line(uint32_t index)
{
    return std::format(
        R"({{"utc":{},"lat":{:.6f},"lon":18.068600,"speed":36.0,"power":-1234,"soc":76}})",
        kUtc + index,
        59.3293 + index * 0.0002);
}

std::string
Trip(uint32_t lines, uint32_t first = 0)
{
    std::string out;
    for (auto i = first; i < first + lines; i++)
    {
        out += Line(i) + "\n";
    }

    return out;
}

// The request for a line of Trip()
std::string
Url(uint32_t index, std::string_view id = "radbuzz")
{
    return std::format("{}/?id={}&timestamp={}&lat={:.6f}&lon=18.068600&speed=19.4&batt=76&"
                       "watts=-1234",
                       kServer,
                       id,
                       kUtc + index,
                       59.3293 + index * 0.0002);
}

std::optional<TraccarUploader::PointRequest>
CreateRequest(std::string_view line)
{
    return TraccarUploader::CreatePointRequest("https://pi", "id", line);
}

class Fixture : public ThreadFixture
{
public:
    // With files which exist before the uploader starts
    explicit Fixture(const std::vector<std::pair<std::string, std::string>>& files = {
                         {TraccarUploader::kConfigurationFile, kConfiguration}})
    {
        mock_filesystem::Clear();
        for (const auto& [path, contents] : files)
        {
            mock_filesystem::SetFile(path, contents);
        }

        // Every request succeeds by default
        m_expectations.push_back(NAMED_ALLOW_CALL(*https_mock, Get(_))
                                     .SIDE_EFFECT(requests.push_back(_1))
                                     .SIDE_EFFECT(progress.push_back(LastSentUtc()))
                                     .SIDE_EFFECT(CHECK(UploadActive()))
                                     .RETURN(std::vector<std::byte> {}));

        state.Set<AS::wifi_connected>(true);

        SetThread(&uploader);
        uploader.Start("traccar_uploader");
    }

    ~Fixture()
    {
        mock_filesystem::Clear();
    }

    bool UploadActive()
    {
        return state.Get<AS::traccar::upload_active>();
    }

    uint32_t LastSentUtc()
    {
        return state.Get<AS::traccar::last_sent_utc>();
    }

    void NewCompactedTrip()
    {
        post_office.Send<MSG::new_compacted_trip>();
        RunLoopWhileReady();
    }

    std::string Contents(std::string_view path) const
    {
        auto data = filesystem.ReadFile(path);
        REQUIRE(data);

        return std::string(reinterpret_cast<const char*>(data->data()), data->size());
    }

    // A request which fails, e.g., since the server can't be reached
    std::unique_ptr<trompeloeil::expectation> FailingRequest(const std::string& url)
    {
        return NAMED_REQUIRE_CALL(*https_mock, Get(url))
            .SIDE_EFFECT(requests.push_back(_1))
            .SIDE_EFFECT(CHECK(UploadActive()))
            .RETURN(std::nullopt);
    }

    std::shared_ptr<mock_https_client::MockHttpsClient> https_mock {mock_https_client::GetMock()};

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;
    Filesystem filesystem {"/sdcard"};
    HttpsClient https_client;
    TraccarUploader uploader {state, post_office, filesystem, https_client};

    std::vector<std::string> requests;
    // LastSentUtc() at each of the (successful) requests
    std::vector<uint32_t> progress;

private:
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

class StateNotifier : public IEventNotifier
{
public:
    MAKE_MOCK0(Notify, void());

private:
    bool NotifyFromIsr() final
    {
        REQUIRE(false); // Forbidden in this test
        return false;
    }
};

class NoConfigurationFixture : public Fixture
{
public:
    NoConfigurationFixture()
        : Fixture({{kLogPath, Trip(2)}})
    {
    }
};

class BrokenConfigurationFixture : public Fixture
{
public:
    BrokenConfigurationFixture()
        : Fixture({{TraccarUploader::kConfigurationFile, R"({"url": "https://pi)"}})
    {
    }
};

class WithoutDemoIdFixture : public Fixture
{
public:
    WithoutDemoIdFixture()
        : Fixture({{TraccarUploader::kConfigurationFile, R"({"url": "https://pi", "id": "a b"})"},
                   {"demo/log/55bbb7a3.jsl", Trip(1)}})
    {
    }
};

class PendingTripFixture : public Fixture
{
public:
    PendingTripFixture()
        : Fixture({{TraccarUploader::kConfigurationFile, kConfiguration}, {kLogPath, Trip(3)}})
    {
    }
};

} // namespace


TEST_SUITE_BEGIN("traccar_uploader");

TEST_CASE("a request is created from a complete trip line")
{
    const auto request = CreateRequest(
        R"({"utc":1774269319,"lat":59.329300,"lon":18.068600,"speed":36.0,"power":-1234,"soc":76})");

    REQUIRE(request);
    CHECK(request->utc == 1774269319);
    // 36 km/h is 19.4 knots
    CHECK(request->url ==
          "https://pi/?id=id&timestamp=1774269319&lat=59.329300&lon=18.068600&speed=19.4&batt=76&"
          "watts=-1234");
}

TEST_CASE("only the time and position are required for a request")
{
    const auto kRequired = "https://pi/?id=id&timestamp=1&lat=-59.500000&lon=-18.000000";

    for (const std::string line : {
             R"({"utc":1,"lat":-59.5,"lon":-18})",
             // Values of the wrong type are skipped
             R"({"utc":1,"lat":-59.5,"lon":-18,"speed":"36","power":"1","soc":"76"})",
             R"({"utc":1,"lat":-59.5,"lon":-18,"speed":null,"power":1.5,"soc":7.5})",
             R"({"utc":1,"lat":-59.5,"lon":-18,"speed":[],"power":{},"soc":true})",
             R"({"utc":1,"lat":-59.5,"lon":-18,"unknown":17})",
             // Windows line ending
             "{\"utc\":1,\"lat\":-59.5,\"lon\":-18}\r",
         })
    {
        CAPTURE(line);
        const auto request = CreateRequest(line);

        REQUIRE(request);
        CHECK(request->utc == 1);
        CHECK(request->url == kRequired);
    }
}

TEST_CASE("the device id is URL encoded in a request")
{
    const auto request = TraccarUploader::CreatePointRequest(
        "https://pi", "aZ09-._~ &=/\xc3\xa5", R"({"utc":1,"lat":1,"lon":2})");

    REQUIRE(request);
    CHECK(request->url ==
          "https://pi/?id=aZ09-._~%20%26%3D%2F%C3%A5&timestamp=1&lat=1.000000&lon=2.000000");
}

TEST_CASE("the largest 32-bit time is valid in a request")
{
    const auto request = CreateRequest(R"({"utc":4294967295,"lat":1,"lon":2})");

    REQUIRE(request);
    CHECK(request->utc == 4294967295);
}

TEST_CASE("no request is created from a broken trip line")
{
    for (const std::string line : {
             "",
             "garbage",
             "{}",
             "[]",
             "[1774269319,59.0,18.0]",
             "17",
             "null",
             R"("utc")",
             // Cut off, e.g., by a power-off
             R"({"utc":1774269319,"lat":59.0,"lon":18.)",
             R"({"utc":1774269319,"lat":59.0,"lon":18.0)",
             R"({"utc":1774269319,"lat":59.0,"lon":18.0}{)",
             // Missing values
             R"({"lat":59.0,"lon":18.0})",
             R"({"utc":1774269319,"lon":18.0})",
             R"({"utc":1774269319,"lat":59.0})",
             // Broken times
             R"({"utc":"1774269319","lat":59.0,"lon":18.0})",
             R"({"utc":null,"lat":59.0,"lon":18.0})",
             R"({"utc":-1,"lat":59.0,"lon":18.0})",
             R"({"utc":1.5,"lat":59.0,"lon":18.0})",
             R"({"utc":4294967296,"lat":59.0,"lon":18.0})",
             R"({"utc":99999999999999999999999,"lat":59.0,"lon":18.0})",
             // Broken positions
             R"({"utc":1774269319,"lat":"59.0","lon":18.0})",
             R"({"utc":1774269319,"lat":59.0,"lon":null})",
             R"({"utc":1774269319,"lat":[59.0],"lon":18.0})",
             R"({"utc":1774269319,"lat":59.0,"lon":1e999})",
             R"({"utc":1774269319,"lat":nan,"lon":18.0})",
         })
    {
        CAPTURE(line);
        CHECK_FALSE(CreateRequest(line));
    }
}

TEST_CASE_FIXTURE(Fixture, "the configuration is imported from traccar.txt")
{
    THEN("the trailing slash of the url is removed")
    {
        CHECK(*state.Get<AS::traccar::configuration>() ==
              TraccarSettings {.url = kServer, .id = "radbuzz", .demo_id = "radbuzz-demo"});
        CHECK(state.Get<AS::traccar::configuration_file_hash>() != 0);
    }
}

TEST_CASE_FIXTURE(NoConfigurationFixture, "nothing is uploaded without a configuration")
{
    NewCompactedTrip();

    CHECK(*state.Get<AS::traccar::configuration>() == TraccarSettings {});
    CHECK(requests.empty());
}

TEST_CASE_FIXTURE(BrokenConfigurationFixture, "a broken traccar.txt isn't imported")
{
    CHECK(*state.Get<AS::traccar::configuration>() == TraccarSettings {});
    CHECK(state.Get<AS::traccar::configuration_file_hash>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "an unchanged traccar.txt isn't imported again")
{
    // E.g., changed on the device, which isn't possible yet
    {
        auto ps = state.CheckoutPartialSnapshot<AS::traccar::configuration>();
        ps.GetWritableReference<AS::traccar::configuration>().id = "other";
    }
    state.Set<AS::traccar::last_sent_utc>(kUtc);

    TraccarUploader second {state, post_office, filesystem, https_client};
    SetThread(&second);
    second.Start("traccar_uploader_2");

    CHECK(state.Get<AS::traccar::configuration>()->id == "other");
    CHECK(LastSentUtc() == kUtc);
}

TEST_CASE_FIXTURE(Fixture, "a changed traccar.txt starts over with a partly uploaded trip")
{
    state.Set<AS::traccar::last_sent_utc>(kUtc + 1);
    mock_filesystem::SetFile(kLogPath, Trip(3));
    mock_filesystem::SetFile(TraccarUploader::kConfigurationFile,
                             R"({"url": "https://other", "id": "new"})");

    TraccarUploader second {state, post_office, filesystem, https_client};
    SetThread(&second);
    second.Start("traccar_uploader_2");
    CHECK(LastSentUtc() == 0);

    RunLoopWhileReady();

    REQUIRE(requests.size() == 3);
    CHECK(requests[0].starts_with(std::format("https://other/?id=new&timestamp={}&", kUtc)));
}

TEST_CASE_FIXTURE(Fixture, "a compacted trip is uploaded, a request per line")
{
    mock_filesystem::SetFile(kLogPath, Trip(2));

    NewCompactedTrip();

    CHECK(requests == std::vector {Url(0), Url(1)});

    THEN("the trip is moved to sent, unchanged")
    {
        CHECK_FALSE(filesystem.FileExists(kLogPath));
        CHECK(Contents(kSentPath) == Trip(2));
    }

    AND_WHEN("another trip is compacted")
    {
        mock_filesystem::SetFile("trips/log/69c14000.jsl", Trip(1, 10));
        requests.clear();

        NewCompactedTrip();

        THEN("only the new trip is uploaded")
        {
            CHECK(requests == std::vector {Url(10)});
            CHECK(filesystem.ListDirectory("trips/log").empty());
            CHECK(filesystem.ListDirectory("trips/sent").size() == 2);
        }
    }
}

TEST_CASE_FIXTURE(PendingTripFixture, "pending trips are uploaded at startup")
{
    RunLoopWhileReady();

    CHECK(requests == std::vector {Url(0), Url(1), Url(2)});
}

TEST_CASE_FIXTURE(Fixture, "demo trips are uploaded with the demo id")
{
    mock_filesystem::SetFile("demo/log/55bbb7a3.jsl", Trip(1));
    mock_filesystem::SetFile(kLogPath, Trip(1, 5));

    NewCompactedTrip();

    CHECK(requests == std::vector {Url(5), Url(0, "radbuzz-demo")});
    CHECK(filesystem.FileExists("demo/sent/55bbb7a3.jsl"));
    CHECK(filesystem.ListDirectory("demo/log").empty());
}

TEST_CASE_FIXTURE(WithoutDemoIdFixture, "demo trips aren't uploaded without a demo id")
{
    mock_filesystem::SetFile(kLogPath, Trip(1));

    NewCompactedTrip();

    THEN("the id is URL encoded")
    {
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].starts_with("https://pi/?id=a%20b&timestamp="));
    }

    AND_THEN("the demo trip is left")
    {
        CHECK(filesystem.FileExists("demo/log/55bbb7a3.jsl"));
    }
}

TEST_CASE_FIXTURE(Fixture, "the upload is shown as active while points are sent")
{
    // Also checked for each request by the fixture
    StateNotifier notifier;
    auto listener = state.AttachListener<AS::traccar::upload_active>(notifier);
    std::vector<bool> changes;

    ALLOW_CALL(notifier, Notify()).LR_SIDE_EFFECT(changes.push_back(UploadActive()));

    WHEN("several trips are uploaded")
    {
        mock_filesystem::SetFile(kLogPath, Trip(3));
        mock_filesystem::SetFile("trips/log/69c14000.jsl", Trip(2, 10));

        NewCompactedTrip();

        THEN("it's active until all of them are done")
        {
            CHECK(requests.size() == 5);
            CHECK(changes == std::vector {true, false});
        }
    }

    WHEN("the upload fails")
    {
        mock_filesystem::SetFile(kLogPath, Trip(3));
        auto failing = FailingRequest(Url(1));

        NewCompactedTrip();

        THEN("it's not active while waiting to retry")
        {
            CHECK(changes == std::vector {true, false});
        }

        AND_WHEN("it's retried")
        {
            failing = nullptr;
            changes.clear();
            AdvanceTimeAndRunLoop(TraccarUploader::kRetryTime);

            THEN("it's active again")
            {
                CHECK(requests.back() == Url(2));
                CHECK(changes == std::vector {true, false});
            }
        }
    }

    WHEN("a trip only has broken lines")
    {
        mock_filesystem::SetFile(kLogPath, "garbage\n");

        NewCompactedTrip();

        THEN("it's never active")
        {
            CHECK(requests.empty());
            CHECK(filesystem.FileExists(kSentPath));
            CHECK(changes.empty());
        }
    }

    WHEN("a trip has been sent completely already")
    {
        mock_filesystem::SetFile(kLogPath, Trip(2));
        state.Set<AS::traccar::last_sent_utc>(kUtc + 1);

        NewCompactedTrip();

        THEN("it's never active")
        {
            CHECK(requests.empty());
            CHECK(filesystem.FileExists(kSentPath));
            CHECK(changes.empty());
        }
    }

    WHEN("there are no trips")
    {
        NewCompactedTrip();

        THEN("it's never active")
        {
            CHECK(changes.empty());
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "nothing is uploaded without wifi")
{
    state.Set<AS::wifi_connected>(false);
    mock_filesystem::SetFile(kLogPath, Trip(2));

    NewCompactedTrip();
    CHECK(requests.empty());

    WHEN("wifi is connected")
    {
        state.Set<AS::wifi_connected>(true);
        RunLoopWhileReady();

        THEN("the trip is uploaded")
        {
            CHECK(requests == std::vector {Url(0), Url(1)});
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a failed upload continues where it failed")
{
    mock_filesystem::SetFile(kLogPath, Trip(5));
    mock_filesystem::SetFile("trips/log/69c14000.jsl", Trip(1, 10));
    auto failing = FailingRequest(Url(2));

    NewCompactedTrip();

    THEN("the upload stops at the failure, also for the other trips")
    {
        CHECK(requests == std::vector {Url(0), Url(1), Url(2)});
        CHECK(filesystem.FileExists(kLogPath));
        CHECK_FALSE(filesystem.FileExists(kSentPath));
        CHECK(LastSentUtc() == kUtc + 1);
    }

    AND_WHEN("the server is reachable again")
    {
        failing = nullptr;
        requests.clear();
        AdvanceTimeAndRunLoop(TraccarUploader::kRetryTime);

        THEN("the rest is uploaded, starting with the failed point")
        {
            CHECK(requests == std::vector {Url(2), Url(3), Url(4), Url(10)});
            CHECK(filesystem.ListDirectory("trips/log").empty());
            CHECK(Contents(kSentPath) == Trip(5));
            CHECK(LastSentUtc() == 0);
        }
    }

    AND_WHEN("the server is still unreachable")
    {
        failing = FailingRequest(Url(2));
        requests.clear();
        AdvanceTimeAndRunLoop(TraccarUploader::kRetryTime);

        THEN("the progress is kept")
        {
            CHECK(requests == std::vector {Url(2)});
            CHECK(LastSentUtc() == kUtc + 1);
        }
    }

    AND_WHEN("wifi is disconnected before the retry")
    {
        failing = nullptr;
        requests.clear();
        state.Set<AS::wifi_connected>(false);
        RunLoopWhileReady();
        AdvanceTimeAndRunLoop(TraccarUploader::kRetryTime);

        THEN("nothing more is sent")
        {
            CHECK(requests.empty());
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the progress is updated regularly")
{
    constexpr auto kInterval = TraccarUploader::kProgressInterval;
    mock_filesystem::SetFile(kLogPath, Trip(2 * kInterval + 1));

    NewCompactedTrip();

    // I.e., when sending the point with that index
    REQUIRE(progress.size() == 2 * kInterval + 1);
    CHECK(progress[kInterval - 1] == 0);
    CHECK(progress[kInterval] == kUtc + kInterval - 1);
    CHECK(progress[2 * kInterval - 1] == kUtc + kInterval - 1);
    CHECK(progress[2 * kInterval] == kUtc + 2 * kInterval - 1);

    THEN("it's reset when the trip is uploaded")
    {
        CHECK(LastSentUtc() == 0);
    }
}

TEST_CASE_FIXTURE(Fixture, "an upload interrupted by a power-off continues")
{
    GIVEN("a partly uploaded trip")
    {
        state.Set<AS::traccar::last_sent_utc>(kUtc + 2);
        mock_filesystem::SetFile(kLogPath, Trip(5));

        NewCompactedTrip();

        THEN("only the rest is sent")
        {
            CHECK(requests == std::vector {Url(3), Url(4)});
            CHECK(Contents(kSentPath) == Trip(5));
            CHECK(LastSentUtc() == 0);
        }
    }

    GIVEN("a trip where only the move to sent is left")
    {
        state.Set<AS::traccar::last_sent_utc>(kUtc + 4);
        mock_filesystem::SetFile(kLogPath, Trip(5));

        NewCompactedTrip();

        THEN("it's moved without sending anything")
        {
            CHECK(requests.empty());
            CHECK(filesystem.FileExists(kSentPath));
            CHECK_FALSE(filesystem.FileExists(kLogPath));
            CHECK(LastSentUtc() == 0);
        }
    }

    GIVEN("the progress of another trip")
    {
        state.Set<AS::traccar::last_sent_utc>(kUtc + 100);
        mock_filesystem::SetFile(kLogPath, Trip(2));

        NewCompactedTrip();

        THEN("the whole trip is sent")
        {
            CHECK(requests == std::vector {Url(0), Url(1)});
        }
    }

    GIVEN("a trip with broken lines")
    {
        state.Set<AS::traccar::last_sent_utc>(kUtc + 1);
        mock_filesystem::SetFile(kLogPath,
                                 "garbage\n" + Line(0) + "\n[]\n{}\n" + R"({"utc":"1"})" + "\n" +
                                     Line(1) + "\ngarbage\n" + Line(2) + "\n");

        NewCompactedTrip();

        THEN("it continues after the last sent point")
        {
            CHECK(requests == std::vector {Url(2)});
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "demo trips also continue, after the real ones")
{
    state.Set<AS::traccar::last_sent_utc>(kUtc + 1);
    mock_filesystem::SetFile("demo/log/55bbb7a3.jsl", Trip(3));

    WHEN("there are only demo trips")
    {
        NewCompactedTrip();

        THEN("the rest of the demo trip is sent")
        {
            CHECK(requests == std::vector {Url(2, "radbuzz-demo")});
            CHECK(LastSentUtc() == 0);
        }
    }

    WHEN("there is also a new real trip")
    {
        mock_filesystem::SetFile(kLogPath, Trip(2, 10));

        NewCompactedTrip();

        THEN("the whole real trip is sent, and the demo trip starts over")
        {
            CHECK(requests == std::vector {Url(10),
                                           Url(11),
                                           Url(0, "radbuzz-demo"),
                                           Url(1, "radbuzz-demo"),
                                           Url(2, "radbuzz-demo")});
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "broken lines are skipped")
{
    mock_filesystem::SetFile(
        kLogPath, Line(0) + "\ngarbage\n" + R"({"utc":17,"lat":59.0})" + "\n" + Line(1) + "\n{}\n");

    NewCompactedTrip();

    CHECK(requests == std::vector {Url(0), Url(1)});
    CHECK(filesystem.FileExists(kSentPath));
}

TEST_CASE_FIXTURE(Fixture, "a sent trip with the same name is replaced")
{
    // E.g., copied back to be uploaded again
    mock_filesystem::SetFile(kSentPath, "old");
    mock_filesystem::SetFile(kLogPath, Trip(2));

    NewCompactedTrip();

    CHECK(requests == std::vector {Url(0), Url(1)});
    CHECK(Contents(kSentPath) == Trip(2));
    CHECK_FALSE(filesystem.FileExists(kLogPath));
}

TEST_SUITE_END();
