#include "mock_filesystem.hh"
#include "test.hh"
#include "thread_fixture.hh"
#include "trip_recorder.hh"
#include "wgs84_to_osm_point.hh"

#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace
{

constexpr uint32_t kUtc = 1774269319;
constexpr GpsPosition kStockholm {59.3293, 18.0686};

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        mock_filesystem::Clear();

        SetThread(&recorder);
        recorder.Start("trip_recorder");

        StartMoving();
    }

    ~Fixture()
    {
        mock_filesystem::Clear();
    }

    // A GPS fix, as written by the GPS reader, moving north a bit every second
    void Fix(uint32_t utc, float speed = 30)
    {
        auto offset = static_cast<float>(utc - kUtc) * 0.0001f;
        FixAt(utc, {kStockholm.latitude + offset, kStockholm.longitude}, speed);
    }

    void FixAt(uint32_t utc, GpsPosition position, float speed = 30)
    {
        {
            // Written on destruction
            auto qw = state.CheckoutQueuedWriter<AS::gps_utc, AS::position, AS::pixel_position>();
            qw.Set<AS::gps_utc>(utc);
            qw.Set<AS::position>({.position = position, .speed = speed, .heading = 90});
            qw.Set<AS::pixel_position>(*Wgs84ToOsmPoint(position, kDefaultZoom));
        }

        RunLoopWhileReady();
    }

    void StartMoving()
    {
        state.Set<AS::is_moving>(true);
        RunLoopWhileReady();
    }

    void Stop()
    {
        state.Set<AS::is_moving>(false);
        RunLoopWhileReady();
    }

    void ResetTrip()
    {
        post_office.Send<MSG::reset_trip>();
        RunLoopWhileReady();
    }

    static std::string TripPath(uint32_t utc)
    {
        return std::format("TRIPS/CURRENT/{:08X}.JSL", utc);
    }

    static std::string RawPath(uint32_t utc)
    {
        return std::format("TRIPS/RAW/{:08X}.JSL", utc);
    }

    static Filesystem::DirectoryEntry Entry(uint32_t utc)
    {
        return {std::format("{:08X}.JSL", utc), false};
    }

    // The lines of a file, or nothing if it doesn't exist
    std::vector<std::string> Lines(std::string_view path) const
    {
        std::vector<std::string> out;

        if (auto data = filesystem.ReadFile(path))
        {
            auto text = std::string(reinterpret_cast<const char*>(data->data()), data->size());
            for (auto line : std::views::split(text, '\n'))
            {
                if (!line.empty())
                {
                    out.emplace_back(line.begin(), line.end());
                }
            }
        }

        return out;
    }

    std::vector<nlohmann::json> JsonLines(std::string_view path) const
    {
        std::vector<nlohmann::json> out;
        for (const auto& line : Lines(path))
        {
            out.push_back(nlohmann::json::parse(line));
        }

        return out;
    }

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;
    Filesystem filesystem {"/sdcard"};
    TripRecorder recorder {state, post_office, filesystem};
};

} // namespace

TEST_SUITE_BEGIN("trip_recorder");

TEST_CASE_FIXTURE(Fixture, "nothing is recorded without a GPS time")
{
    for (auto i = 0; i < 100; i++)
    {
        FixAt(0, {kStockholm.latitude + i * 0.0001f, kStockholm.longitude});
    }
    Stop();

    CHECK(filesystem.ListDirectory("").empty());
}

TEST_CASE_FIXTURE(Fixture, "nothing is recorded while standing still")
{
    Stop();

    Fix(kUtc);
    Fix(kUtc + 1);
    // E.g., if the GPS sees no movement
    StartMoving();
    Stop();

    CHECK(filesystem.ListDirectory("").empty());

    AND_WHEN("the vehicle starts moving")
    {
        StartMoving();
        Fix(kUtc + 2);
        Stop();

        THEN("the trip starts at the first fix while moving")
        {
            auto json = JsonLines(TripPath(kUtc + 2));
            REQUIRE(json.size() == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "GPS fixes are recorded as JSON lines")
{
    state.Set<AS::current_power_w>(-1234);
    state.Set<AS::battery_soc>(76);

    Fix(kUtc);
    FixAt(kUtc + 1, {-33.935, -18.4245}, 12.25);

    THEN("nothing is written while moving, with a buffer which isn't full")
    {
        CHECK_FALSE(filesystem.FileExists(TripPath(kUtc)));
    }

    AND_WHEN("the vehicle stops")
    {
        Stop();

        THEN("the lines are written to a file named by the time of the first fix")
        {
            CHECK(filesystem.ListDirectory("TRIPS/CURRENT") ==
                  std::vector<Filesystem::DirectoryEntry> {{"69C13387.JSL", false}});

            auto lines = Lines(TripPath(kUtc));
            REQUIRE(lines.size() == 2);
            CHECK(lines[0] == R"({"utc":1774269319,"lat":59.329300,"lon":18.068600,)"
                              R"("speed":30.0,"power":-1234,"soc":76})");
        }

        THEN("the lines are valid JSON")
        {
            auto json = JsonLines(TripPath(kUtc));
            REQUIRE(json.size() == 2);

            CHECK(json[1]["utc"] == kUtc + 1);
            CHECK(json[1]["lat"] == doctest::Approx(-33.935));
            CHECK(json[1]["lon"] == doctest::Approx(-18.4245));
            CHECK(json[1]["speed"] == doctest::Approx(12.2).epsilon(0.01));
            CHECK(json[1]["power"] == -1234);
            CHECK(json[1]["soc"] == 76);
        }

        AND_WHEN("it starts moving again")
        {
            StartMoving();
            Fix(kUtc + 100);
            Stop();

            THEN("the trip continues in the same file")
            {
                auto json = JsonLines(TripPath(kUtc));
                REQUIRE(json.size() == 3);
                CHECK(json[2]["utc"] == kUtc + 100);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the lines are written when the buffer is full")
{
    // Fill the buffer
    auto utc = kUtc;
    while (!filesystem.FileExists(TripPath(kUtc)))
    {
        Fix(utc++);
        REQUIRE(utc - kUtc < TripRecorder::kBufferSize);
    }

    auto written = Lines(TripPath(kUtc)).size();
    CHECK(written == utc - kUtc);

    AND_WHEN("more fixes arrive, and the vehicle stops")
    {
        Fix(utc++);
        Fix(utc++);
        Stop();

        THEN("they are appended, in order")
        {
            auto json = JsonLines(TripPath(kUtc));
            REQUIRE(json.size() == written + 2);
            for (auto i = 0u; i < json.size(); i++)
            {
                CHECK(json[i]["utc"] == kUtc + i);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "fixes without a new GPS time are skipped")
{
    // E.g., a position update without an RMC sentence
    Fix(kUtc);
    FixAt(kUtc, {1, 2}, 40);
    Fix(kUtc + 1);
    Stop();

    auto json = JsonLines(TripPath(kUtc));
    REQUIRE(json.size() == 2);
    CHECK(json[0]["speed"] == 30.0);
}

TEST_CASE_FIXTURE(Fixture, "nothing is recorded in demo mode")
{
    state.Set<AS::demo_mode>(true);

    Fix(kUtc);
    Fix(kUtc + 1);
    Stop();

    CHECK(filesystem.ListDirectory("").empty());

    AND_WHEN("demo mode ends")
    {
        state.Set<AS::demo_mode>(false);
        StartMoving();
        Fix(kUtc + 2);
        Stop();

        THEN("the trip starts at the first real fix")
        {
            auto json = JsonLines(TripPath(kUtc + 2));
            REQUIRE(json.size() == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "an unchanged pixel position isn't recorded")
{
    // Less than a meter apart, i.e., GPS noise
    const GpsPosition nearby {kStockholm.latitude, kStockholm.longitude + 0.000005f};
    REQUIRE(!(nearby == kStockholm));
    REQUIRE(*Wgs84ToOsmPoint(nearby, kDefaultZoom) == *Wgs84ToOsmPoint(kStockholm, kDefaultZoom));

    FixAt(kUtc, kStockholm);
    FixAt(kUtc + 1, kStockholm);
    FixAt(kUtc + 2, nearby);
    Stop();

    CHECK(JsonLines(TripPath(kUtc)).size() == 1);
}

TEST_CASE_FIXTURE(Fixture, "stopping without any fixes writes nothing")
{
    Stop();
    StartMoving();
    Stop();

    CHECK(filesystem.ListDirectory("").empty());
}

TEST_CASE_FIXTURE(Fixture, "a trip reset moves the trip to RAW")
{
    using Entries = std::vector<Filesystem::DirectoryEntry>;

    Fix(kUtc);
    Fix(kUtc + 1);

    WHEN("the trip is reset while moving")
    {
        ResetTrip();

        THEN("the buffered lines are written, and the trip is moved")
        {
            CHECK(filesystem.ListDirectory("TRIPS/CURRENT").empty());
            CHECK(filesystem.ListDirectory("TRIPS/RAW") == Entries {Entry(kUtc)});
            CHECK(JsonLines(RawPath(kUtc)).size() == 2);
        }

        AND_WHEN("more fixes arrive")
        {
            Fix(kUtc + 10);
            Fix(kUtc + 11);
            Stop();

            THEN("they start a new trip, and the old one is unchanged")
            {
                CHECK(filesystem.ListDirectory("TRIPS/CURRENT") == Entries {Entry(kUtc + 10)});
                CHECK(JsonLines(TripPath(kUtc + 10)).size() == 2);
                CHECK(JsonLines(RawPath(kUtc)).size() == 2);
            }

            AND_WHEN("that trip is reset as well")
            {
                ResetTrip();

                THEN("both trips are in RAW")
                {
                    CHECK(filesystem.ListDirectory("TRIPS/CURRENT").empty());
                    CHECK(filesystem.ListDirectory("TRIPS/RAW") ==
                          Entries {Entry(kUtc), Entry(kUtc + 10)});
                }
            }
        }
    }

    WHEN("the trip is reset after stopping")
    {
        Stop();
        REQUIRE(filesystem.FileExists(TripPath(kUtc)));

        ResetTrip();

        THEN("the written trip is moved")
        {
            CHECK_FALSE(filesystem.FileExists(TripPath(kUtc)));
            CHECK(JsonLines(RawPath(kUtc)).size() == 2);
        }
    }

    WHEN("the trip is reset twice")
    {
        ResetTrip();
        ResetTrip();

        THEN("the second reset does nothing")
        {
            CHECK(filesystem.ListDirectory("TRIPS/CURRENT").empty());
            CHECK(filesystem.ListDirectory("TRIPS/RAW") == Entries {Entry(kUtc)});
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a trip reset without a trip does nothing")
{
    ResetTrip();

    CHECK(filesystem.ListDirectory("").empty());

    AND_WHEN("a fix arrives")
    {
        Fix(kUtc);
        Stop();

        THEN("a trip is started as usual")
        {
            CHECK(JsonLines(TripPath(kUtc)).size() == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a trip which can't be moved is left in CURRENT")
{
    // E.g., a trip with the same name from before
    mock_filesystem::SetFile(RawPath(kUtc), "old");

    Fix(kUtc);
    ResetTrip();

    CHECK(JsonLines(TripPath(kUtc)).size() == 1);
    CHECK(Lines(RawPath(kUtc)) == std::vector<std::string> {"old"});

    AND_WHEN("more fixes arrive")
    {
        Fix(kUtc + 10);
        Stop();

        THEN("they still start a new trip")
        {
            CHECK(JsonLines(TripPath(kUtc)).size() == 1);
            CHECK(JsonLines(TripPath(kUtc + 10)).size() == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "lines which can't be written are dropped")
{
    // A file in the way of the directory
    mock_filesystem::SetFile("TRIPS/CURRENT", "in the way");

    Fix(kUtc);
    Stop();

    CHECK_FALSE(filesystem.FileExists(TripPath(kUtc)));

    AND_WHEN("the problem goes away")
    {
        REQUIRE(filesystem.Remove("TRIPS/CURRENT"));

        StartMoving();
        Fix(kUtc + 1);
        Stop();

        THEN("only the new lines are written")
        {
            auto json = JsonLines(TripPath(kUtc));
            REQUIRE(json.size() == 1);
            CHECK(json[0]["utc"] == kUtc + 1);
        }
    }
}

TEST_SUITE_END();
