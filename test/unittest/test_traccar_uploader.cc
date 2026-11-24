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
                                     .RETURN(std::vector<std::byte> {}));

        state.Set<AS::wifi_connected>(true);

        SetThread(&uploader);
        uploader.Start("traccar_uploader");
    }

    ~Fixture()
    {
        mock_filesystem::Clear();
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
            .RETURN(std::nullopt);
    }

    std::shared_ptr<mock_https_client::MockHttpsClient> https_mock {mock_https_client::GetMock()};

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;
    Filesystem filesystem {"/sdcard"};
    HttpsClient https_client;
    TraccarUploader uploader {state, post_office, filesystem, https_client};

    std::vector<std::string> requests;

private:
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
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

TEST_CASE_FIXTURE(Fixture, "the configuration is imported from traccar.txt")
{
    THEN("the trailing slash of the url is removed")
    {
        CHECK(*state.Get<AS::configuration::traccar>() ==
              TraccarSettings {.url = kServer, .id = "radbuzz", .demo_id = "radbuzz-demo"});
        CHECK(state.Get<AS::configuration::traccar_file_hash>() != 0);
    }
}

TEST_CASE_FIXTURE(NoConfigurationFixture, "nothing is uploaded without a configuration")
{
    NewCompactedTrip();

    CHECK(*state.Get<AS::configuration::traccar>() == TraccarSettings {});
    CHECK(requests.empty());
}

TEST_CASE_FIXTURE(BrokenConfigurationFixture, "a broken traccar.txt isn't imported")
{
    CHECK(*state.Get<AS::configuration::traccar>() == TraccarSettings {});
    CHECK(state.Get<AS::configuration::traccar_file_hash>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "an unchanged traccar.txt isn't imported again")
{
    // E.g., changed on the device, which isn't possible yet
    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration::traccar>();
        ps.GetWritableReference<AS::configuration::traccar>().id = "other";
    }

    TraccarUploader second {state, post_office, filesystem, https_client};
    SetThread(&second);
    second.Start("traccar_uploader_2");

    CHECK(state.Get<AS::configuration::traccar>()->id == "other");
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

TEST_CASE_FIXTURE(Fixture, "a failed upload is retried from the start")
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
    }

    AND_WHEN("the server is reachable again")
    {
        failing = nullptr;
        requests.clear();
        AdvanceTimeAndRunLoop(TraccarUploader::kRetryTime);

        THEN("everything is uploaded, the failed trip from the start")
        {
            CHECK(requests == std::vector {Url(0), Url(1), Url(2), Url(3), Url(4), Url(10)});
            CHECK(filesystem.ListDirectory("trips/log").empty());
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

TEST_CASE_FIXTURE(Fixture, "broken lines are skipped")
{
    mock_filesystem::SetFile(kLogPath,
                             Line(0) + "\ngarbage\n{}\n" + R"({"utc":"1","lat":59.0,"lon":18.0})" +
                                 "\n" + Line(1) + "\n" +
                                 // Only the required values
                                 R"({"utc":1774269400,"lat":59.0,"lon":18.0})" + "\n");

    NewCompactedTrip();

    CHECK(requests ==
          std::vector<std::string> {
              Url(0),
              Url(1),
              std::format("{}/?id=radbuzz&timestamp=1774269400&lat=59.000000&lon=18.000000",
                          kServer)});
    CHECK(filesystem.FileExists(kSentPath));
}

TEST_CASE_FIXTURE(Fixture, "a sent trip with the same name is replaced")
{
    // E.g., a demo trip from an earlier run
    mock_filesystem::SetFile(kSentPath, "old");
    mock_filesystem::SetFile(kLogPath, Trip(2));

    NewCompactedTrip();

    CHECK(requests == std::vector {Url(0), Url(1)});
    CHECK(Contents(kSentPath) == Trip(2));
    CHECK_FALSE(filesystem.FileExists(kLogPath));
}

TEST_SUITE_END();
