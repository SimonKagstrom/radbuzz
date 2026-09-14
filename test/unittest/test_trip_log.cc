#include "test.hh"
#include "trip_log.hh"

#include <ranges>
namespace
{

constexpr auto kNumberOfTripLogEntries = 16;
constexpr auto kTripLogSize = 6;

class Allocator : public IEntryAllocator
{
public:
    Allocator()
    {
        for (LogHandle handle = 0; handle < kNumberOfTripLogEntries; ++handle)
        {
            m_free_log_entries.push_back(handle);
        }

        m_expectations.push_back(
            NAMED_ALLOW_CALL(*this, AllocateLogEntry()).RETURN(DoAllocateLogEntry()));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(*this, FreeLogEntry(_)).SIDE_EFFECT(DoFreeLogEntry(_1)));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(*this, WritableEntry(_)).RETURN(DoWritableEntry(_1)));
        m_expectations.push_back(NAMED_ALLOW_CALL(*this, Entry(_)).RETURN(DoEntry(_1)));
    }

    MAKE_MOCK0(AllocateLogEntry, std::optional<LogHandle>(), final);
    MAKE_MOCK1(FreeLogEntry, void(LogHandle handle), final);
    MAKE_MOCK1(WritableEntry, TripLogEntry&(LogHandle handle), final);
    MAKE_CONST_MOCK1(Entry, const TripLogEntry&(LogHandle handle), final);

private:
    std::optional<LogHandle> DoAllocateLogEntry()
    {
        if (m_free_log_entries.empty())
        {
            return std::nullopt;
        }

        auto handle = m_free_log_entries.back();
        m_free_log_entries.pop_back();

        return handle;
    }

    void DoFreeLogEntry(LogHandle handle)
    {
        REQUIRE(handle != kInvalidLogHandle);

        m_free_log_entries.push_back(handle);
    }

    TripLogEntry& DoWritableEntry(LogHandle handle)
    {
        return m_trip_log_storage[handle];
    }

    const TripLogEntry& DoEntry(LogHandle handle) const
    {
        return m_trip_log_storage[handle];
    }

    std::array<TripLogEntry, kNumberOfTripLogEntries> m_trip_log_storage;
    std::vector<LogHandle> m_free_log_entries;


    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

class Fixture
{
public:
    Fixture()
    {
    }

    std::vector<LogHandle> WalkBack(LogHandle start)
    {
        std::vector<LogHandle> path;
        auto current = start;
        while (current != kInvalidLogHandle)
        {
            path.push_back(current);
            auto& entry = allocator.Entry(current);
            if (entry.stale)
            {
                printf("Handle %d stale, is (%d,%d)\n", current, entry.position.x, entry.position.y);
            }
            CHECK(entry.stale == false);
            current = entry.predecessor;
        }
        return path;
    }

    struct PointAndTriangleArea
    {
        uint16_t handle;
        Point point;
        TriangleAreaType triangle_area;
    };

    auto PathToPoints(LogHandle start)
    {
        auto path = WalkBack(start);
        std::vector<PointAndTriangleArea> point_path;

        for (auto handle : path)
        {
            auto& entry = allocator.Entry(handle);
            TriangleAreaType area =
                entry.successor == kInvalidLogHandle ? 65535 : trip_log.TriangleArea(entry);

            point_path.push_back({handle, entry.position, area});
        }

        return point_path;
    }

    static constexpr Point P(auto x, auto y)
    {
        return Point {x, y, kDefaultZoom};
    }

    Allocator allocator;
    TripLog<kTripLogSize> trip_log {allocator};
};

} // namespace

TEST_SUITE_BEGIN("trip_log");

TEST_CASE_FIXTURE(Fixture, "the trip log is empty by default")
{
    CHECK(trip_log.GetLastHandle() == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "an entry can be added to the trip log")
{
    auto handle = trip_log.AddEntry(P(0, 0), 10ms, 100);

    REQUIRE(handle.has_value());
    CHECK(trip_log.GetLastHandle() == handle);

    auto& entry = allocator.Entry(*handle);
    CHECK(entry.position == Point {0, 0, kDefaultZoom});
    CHECK(entry.timestamp == 10ms);
    CHECK(entry.power == 100);
    // The first entry
    CHECK(entry.predecessor == kInvalidLogHandle);
    CHECK(entry.successor == kInvalidLogHandle);
}

TEST_CASE_FIXTURE(Fixture, "closeness of entries determine if they are added")
{
    GIVEN("an entry in the trip log")
    {
        auto h0 = trip_log.AddEntry(P(0, 0), 10ms, 100);
        REQUIRE(h0.has_value());

        WHEN("a very close entry is added")
        {
            auto h1 = trip_log.AddEntry(P(4, 4), 11ms, 100);
            THEN("it's not added")
            {
                CHECK(!h1.has_value());
            }
        }

        WHEN("an entry further away is added")
        {
            // On a straight line
            auto h1 = trip_log.AddEntry(P(0, 6), 11ms, 100);
            THEN("it can be added")
            {
                REQUIRE(h1.has_value());
                auto& entry0 = allocator.Entry(*h0);
                auto& entry1 = allocator.Entry(*h1);
                CHECK(entry0.successor == *h1);
                CHECK(entry1.predecessor == *h0);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "points in the trip log are pruned depending on angle change")
{
    std::vector<LogHandle> handles;

    if (0) //GIVEN("points along a straight line filling the log")
    {
        // kTripLogSize in storage plus one in the hand
        for (auto i = 0; i < kTripLogSize + 1; i++)
        {
            // On a line downward
            auto handle = trip_log.AddEntry(P(0, i * 10), 10ms, 100);
            REQUIRE(handle.has_value());
            handles.push_back(*handle);
        }
        auto first_path = WalkBack(handles.back());
        CHECK(first_path.size() == kTripLogSize + 1);

        WHEN("a new entry with an edge is added")
        {
            auto& last = allocator.Entry(handles.back());
            // To the right
            auto handle = trip_log.AddEntry(P(10, last.position.y), 10ms, 100);
            REQUIRE(handle.has_value());

            THEN("one of the previous entries is pruned")
            {
                auto second_path = WalkBack(*handle);
                CHECK(second_path.size() == first_path.size());

                CHECK(second_path.front() == *handle);
                CHECK(std::ranges::find(first_path, *handle) == first_path.end());
            }

            AND_WHEN("another edgy entry is added")
            {
                auto& last = allocator.Entry(*handle);
                // Down from the above
                auto next_handle = trip_log.AddEntry(P(10, last.position.y + 10), 10ms, 100);
                REQUIRE(next_handle.has_value());

                THEN("the last added edgy is also kept")
                {
                    auto path = WalkBack(*next_handle);
                    CHECK(path.front() == *next_handle);
                    CHECK(std::ranges::find(path, *handle) != path.end());
                }
            }
        }
    }

    GIVEN("points that require area recalculation")
    {
        // Gemini gave these values as an example where removing points would require neighbor recalculation

        constexpr auto kLine = std::array {
            /* 0 */ P(0, 0),
            /* 1 */ P(10, 10),
            /* 2 */ P(20, 100), // A massive spike
            /* 3 */ P(30, 101), // Right next to the spike, lowest area
            /* 4 */ P(40, 103), // Second lowest area in initial calculation
            /* 5 */ P(50, 0),
            /* 6 */ P(60, 100), // Should always be kept, but make sure the end is edgy
        };

        const auto first_expected_removal = kLine[3];
        const auto second_expected_removal = kLine[1];

        for (auto& p : kLine)
        {
            auto handle = trip_log.AddEntry(p, 10ms, 100);
            REQUIRE(handle.has_value());
            handles.push_back(*handle);
        }

        WHEN("another entry is added")
        {
            auto path = PathToPoints(handles.back());
            for (auto p : path)
            {
                printf("%02d: (%u, %u) -> %u\n", p.handle, p.point.x, p.point.y, p.triangle_area);
            }
            printf("------\n");
            auto new_handle = trip_log.AddEntry(P(70, 0), 10ms, 100);
            REQUIRE(new_handle.has_value());

            //THEN("the first expected removal is pruned")
            {
                auto path = PathToPoints(*new_handle);
                for (auto point : path)
                {
                    printf("%02d: (%u, %u) -> %u\n",
                           point.handle,
                           point.point.x,
                           point.point.y,
                           point.triangle_area);
                }
                CHECK(std::ranges::find_if(path, [&](auto& p) {
                          return p.point == first_expected_removal;
                      }) == path.end());
            }

            AND_WHEN("yet another is added")
            {
                auto new_handle = trip_log.AddEntry(P(80, 100), 10ms, 100);
                REQUIRE(new_handle.has_value());

                THEN("the second smallest should be removed")
                {
                    auto path = PathToPoints(*new_handle);
                    printf("::::::::::::::\n");
                    for (auto p : path)
                    {
                        printf("%02d: (%u, %u) -> %u\n",
                               p.handle,
                               p.point.x,
                               p.point.y,
                               p.triangle_area);
                    }
                    CHECK(std::ranges::find_if(path, [&](auto& p) {
                              return p.point == second_expected_removal;
                          }) == path.end());
                }
            }
        }
    }
}

TEST_SUITE_END();
