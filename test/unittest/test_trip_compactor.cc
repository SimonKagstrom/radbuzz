#include "job_pool_thread.hh"
#include "mock_filesystem.hh"
#include "test.hh"
#include "thread_fixture.hh"
#include "trip_compactor.hh"
#include "trip_recorder.hh"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <vector>

namespace
{

constexpr uint32_t kUtc = 1774269319;
constexpr auto kName = "69c13387.jsl";
constexpr auto kRawPath = "trips/raw/69c13387.jsl";
constexpr auto kLogPath = "trips/log/69c13387.jsl";
constexpr auto kRawDirectory = "trips/raw";
constexpr auto kLogDirectory = "trips/log";

// A line as written by the TripRecorder
std::string
Line(uint32_t utc, double latitude, double longitude, int power = -1234)
{
    return std::format(
        R"({{"utc":{},"lat":{:.6f},"lon":{:.6f},"speed":30.0,"power":{},"soc":76}})",
        utc,
        latitude,
        longitude,
        power);
}

// Moving north, about 22 m (9 pixels) per point, so no point is too close to the previous one
std::string
NorthLine(uint32_t index)
{
    return Line(kUtc + index, 59.3293 + index * 0.0002, 18.0686);
}

// Lines wiggling east and west while going north, with pseudo-random but deterministic offsets
std::vector<std::string>
WigglyTrip(uint32_t count)
{
    std::vector<std::string> out;

    auto seed = 12345u;
    for (auto i = 0u; i < count; i++)
    {
        seed = seed * 1103515245u + 12345u;
        auto offset = static_cast<double>((seed >> 16) % 600) * 0.000001 - 0.0003;

        out.push_back(Line(kUtc + i, 59.3293 + i * 0.0002, 18.0686 + offset));
    }

    return out;
}

std::string
Join(const std::vector<std::string>& lines)
{
    std::string out;
    for (const auto& line : lines)
    {
        out += line + "\n";
    }

    return out;
}

// True if all of sub is in lines, in the same order
bool
IsSubsequence(const std::vector<std::string>& sub, const std::vector<std::string>& lines)
{
    auto it = lines.begin();
    for (const auto& line : sub)
    {
        it = std::find(it, lines.end(), line);
        if (it == lines.end())
        {
            return false;
        }
        ++it;
    }

    return true;
}

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
    // With files which exist before the compactor starts
    explicit Fixture(const std::vector<std::pair<std::string, std::string>>& files = {})
    {
        mock_filesystem::Clear();
        for (const auto& [path, contents] : files)
        {
            mock_filesystem::SetFile(path, contents);
        }

        // Pooled, like on the target. The job pool owns it from here on
        job_pool.AttachPooledThread(std::make_unique<TripCompactor>(post_office, filesystem));
        SetThread(&job_pool);

        job_pool.Start("job_pool");
    }

    ~Fixture()
    {
        mock_filesystem::Clear();
    }

    // The number of new_compacted_trip messages since the last call
    unsigned NewCompactedTrips()
    {
        auto count = 0u;
        m_mailbox->Collect().On<MSG::new_compacted_trip>([&count](auto) { count++; });

        return count;
    }

    void NewRecordedTrip()
    {
        post_office.Send<MSG::new_recorded_trip>();
        RunLoopWhileReady();
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

    PostOffice<MSG::AllMessages> post_office;
    Filesystem filesystem {"/sdcard"};

private:
    // Before the job pool, so that messages sent at startup are received
    NullNotifier m_notifier;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox {
        post_office.Subscribe<MSG::new_compacted_trip>(m_notifier)};

public:
    JobPoolThread job_pool;
};

// Trips left in raw from before a restart
class StartupFixture : public Fixture
{
public:
    StartupFixture()
        : Fixture({{kRawPath, Join({NorthLine(0), NorthLine(1)})},
                   {"trips/raw/69c14000.jsl", Join({NorthLine(100)})}})
    {
    }
};

} // namespace


TEST_SUITE_BEGIN("trip_compactor");

TEST_CASE_FIXTURE(StartupFixture, "trips left in raw are compacted at startup")
{
    RunLoopWhileReady();

    CHECK(Lines(kLogPath) == std::vector {NorthLine(0), NorthLine(1)});
    CHECK(Lines("trips/log/69c14000.jsl") == std::vector {NorthLine(100)});
    CHECK(filesystem.ListDirectory(kRawDirectory).empty());
    CHECK(NewCompactedTrips() == 2);
}

TEST_CASE_FIXTURE(Fixture, "trips added after startup are only compacted on new recorded trips")
{
    mock_filesystem::SetFile(kRawPath, Join({NorthLine(0), NorthLine(1)}));
    RunLoopWhileReady();

    REQUIRE(filesystem.FileExists(kRawPath));
    REQUIRE(filesystem.ListDirectory(kLogDirectory).empty());
    REQUIRE(NewCompactedTrips() == 0);

    WHEN("a new trip has been recorded")
    {
        NewRecordedTrip();

        THEN("the trip is moved to the log, and removed from raw")
        {
            CHECK(Lines(kLogPath) == std::vector {NorthLine(0), NorthLine(1)});
            CHECK_FALSE(filesystem.FileExists(kRawPath));
        }

        AND_THEN("the new compacted trip is announced")
        {
            CHECK(NewCompactedTrips() == 1);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a short trip is copied unchanged")
{
    auto lines = std::vector<std::string> {};
    for (auto i = 0u; i < 20; i++)
    {
        lines.push_back(NorthLine(i));
    }
    mock_filesystem::SetFile(kRawPath, Join(lines));

    NewRecordedTrip();

    CHECK(Lines(kLogPath) == lines);
}

TEST_CASE_FIXTURE(Fixture, "a long trip is compacted to at most kMaxEntries lines")
{
    auto lines = WigglyTrip(TripCompactor::kMaxEntries * 8);
    mock_filesystem::SetFile(kRawPath, Join(lines));

    NewRecordedTrip();

    auto compacted = Lines(kLogPath);
    REQUIRE(compacted.size() == TripCompactor::kMaxEntries);

    THEN("the lines are unchanged and in the same order as in the raw trip")
    {
        CHECK(IsSubsequence(compacted, lines));
    }

    AND_THEN("the first and the last point are kept")
    {
        CHECK(compacted.front() == lines.front());
        CHECK(compacted.back() == lines.back());
    }

    AND_THEN("the raw trip is removed")
    {
        CHECK_FALSE(filesystem.FileExists(kRawPath));
    }
}

TEST_CASE_FIXTURE(Fixture, "a straight trip keeps only the corners")
{
    // North, then east, with points on straight lines in between
    auto lines = std::vector<std::string> {};
    for (auto i = 0u; i < TripCompactor::kMaxEntries; i++)
    {
        lines.push_back(NorthLine(i));
    }
    auto corner = lines.back();
    for (auto i = 1u; i <= TripCompactor::kMaxEntries; i++)
    {
        lines.push_back(Line(kUtc + TripCompactor::kMaxEntries + i,
                             59.3293 + (TripCompactor::kMaxEntries - 1) * 0.0002,
                             18.0686 + i * 0.0004));
    }
    mock_filesystem::SetFile(kRawPath, Join(lines));

    NewRecordedTrip();

    auto compacted = Lines(kLogPath);
    REQUIRE(compacted.size() == TripCompactor::kMaxEntries);
    CHECK(IsSubsequence(compacted, lines));
    CHECK(std::ranges::find(compacted, corner) != compacted.end());
}

TEST_CASE_FIXTURE(Fixture, "points close to the previous one are dropped")
{
    // Standing still for a few seconds, then moving on
    mock_filesystem::SetFile(kRawPath,
                             Join({Line(kUtc, 59.3293, 18.0686),
                                   Line(kUtc + 1, 59.3293, 18.0686),
                                   Line(kUtc + 2, 59.32931, 18.0686),
                                   NorthLine(10)}));

    NewRecordedTrip();

    CHECK(Lines(kLogPath) == std::vector {Line(kUtc, 59.3293, 18.0686), NorthLine(10)});
}

TEST_CASE_FIXTURE(Fixture, "broken lines are skipped")
{
    auto without_power =
        std::format(R"({{"utc":{},"lat":59.330100,"lon":18.068600,"soc":76}})", kUtc + 4);
    auto large_power = Line(kUtc + 5, 59.3303, 18.0686, 100000);

    mock_filesystem::SetFile(
        kRawPath,
        Join({NorthLine(0),
              "",
              "garbage",
              "[1, 2, 3]",
              "{}",
              R"({"utc":"1774269320","lat":59.3295,"lon":18.0686})",
              R"({"utc":-1,"lat":59.3295,"lon":18.0686})",
              R"({"utc":4294967296,"lat":59.3295,"lon":18.0686})",
              R"({"utc":1774269320,"lon":18.0686})",
              R"({"utc":1774269320,"lat":59.3295})",
              R"({"utc":1774269320,"lat":"north","lon":18.0686})",
              R"({"utc":1774269320,"lat":59.3295,"lon":null})",
              R"({"utc":1774269320,"lat":86.0,"lon":18.0686})",
              R"({"utc":1774269320,"lat":59.3295,"lon":-180.5})",
              NorthLine(2),
              without_power,
              large_power}) +
            // Cut off by a power loss
            R"({"utc":1774269330,"lat":59.33)");

    NewRecordedTrip();

    CHECK(Lines(kLogPath) == std::vector {NorthLine(0), NorthLine(2), without_power, large_power});
    CHECK_FALSE(filesystem.FileExists(kRawPath));
}

TEST_CASE_FIXTURE(Fixture, "a complete last line without a newline is kept")
{
    mock_filesystem::SetFile(kRawPath, NorthLine(0) + "\n" + NorthLine(1));

    NewRecordedTrip();

    CHECK(filesystem.ReadFile(kLogPath).has_value());
    CHECK(Lines(kLogPath) == std::vector {NorthLine(0), NorthLine(1)});
}

TEST_CASE_FIXTURE(Fixture, "a trip without any points is removed without a log")
{
    mock_filesystem::SetFile(kRawPath, "garbage\n{}\n");

    NewRecordedTrip();

    CHECK_FALSE(filesystem.FileExists(kRawPath));
    CHECK(filesystem.ListDirectory(kLogDirectory).empty());

    AND_THEN("nothing is announced")
    {
        CHECK(NewCompactedTrips() == 0);
    }
}

TEST_CASE_FIXTURE(Fixture, "all trips in raw are compacted")
{
    constexpr auto kOtherRawPath = "trips/raw/69c14000.jsl";
    constexpr auto kOtherLogPath = "trips/log/69c14000.jsl";
    constexpr auto kInSubdirectory = "trips/raw/old/69c10000.jsl";

    mock_filesystem::SetFile(kRawPath, Join({NorthLine(0), NorthLine(1)}));
    mock_filesystem::SetFile(kOtherRawPath, Join({NorthLine(100), NorthLine(101)}));
    mock_filesystem::SetFile(kInSubdirectory, Join({NorthLine(200)}));

    NewRecordedTrip();

    CHECK(Lines(kLogPath) == std::vector {NorthLine(0), NorthLine(1)});
    CHECK(Lines(kOtherLogPath) == std::vector {NorthLine(100), NorthLine(101)});
    CHECK(filesystem.ListDirectory(kRawDirectory) ==
          std::vector<Filesystem::DirectoryEntry> {{"old", true}});
    CHECK(NewCompactedTrips() == 2);

    AND_THEN("directories are left alone")
    {
        CHECK(filesystem.FileExists(kInSubdirectory));
    }
}

TEST_CASE_FIXTURE(Fixture, "demo trips are compacted to the demo log")
{
    mock_filesystem::SetFile("demo/raw/69c13387.jsl", Join({NorthLine(0), NorthLine(1)}));
    mock_filesystem::SetFile(kRawPath, Join({NorthLine(100)}));

    NewRecordedTrip();

    CHECK(Lines("demo/log/69c13387.jsl") == std::vector {NorthLine(0), NorthLine(1)});
    CHECK(Lines(kLogPath) == std::vector {NorthLine(100)});
    CHECK(filesystem.ListDirectory("demo/raw").empty());
    CHECK(filesystem.ListDirectory(kRawDirectory).empty());
    CHECK(NewCompactedTrips() == 2);
}

TEST_CASE_FIXTURE(Fixture, "an existing log is replaced")
{
    mock_filesystem::SetFile(kLogPath, "old\nlines\nwhich\nare\nlonger\n");
    mock_filesystem::SetFile(kRawPath, Join({NorthLine(0)}));

    NewRecordedTrip();

    CHECK(Lines(kLogPath) == std::vector {NorthLine(0)});
}

TEST_CASE_FIXTURE(Fixture, "a trip which can't be written is left in raw")
{
    mock_filesystem::SetFile(kRawPath, Join({NorthLine(0), NorthLine(1)}));
    mock_filesystem::SetFile(kLogDirectory, "in the way");

    NewRecordedTrip();

    CHECK(filesystem.FileExists(kRawPath));
    CHECK(NewCompactedTrips() == 0);

    WHEN("it can be written on the next new trip")
    {
        REQUIRE(filesystem.Remove(kLogDirectory));
        NewRecordedTrip();

        THEN("it's compacted")
        {
            CHECK(Lines(kLogPath) == std::vector {NorthLine(0), NorthLine(1)});
            CHECK_FALSE(filesystem.FileExists(kRawPath));
            CHECK(NewCompactedTrips() == 1);
        }
    }
}

TEST_SUITE_END();
