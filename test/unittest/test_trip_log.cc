#include "test.hh"
#include "trip_log.hh"

#include <ranges>
namespace
{

constexpr auto kNumberOfTripLogEntries = 32;
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
            if (allocator.Entry(current).stale)
            {
                printf("Encountered a stale entry at handle %u (%u, %u)\n",
                       current,
                       allocator.Entry(current).position.x,
                       allocator.Entry(current).position.y);
                for (auto h : path)
                {
                    printf("  -> handle %u (%u, %u)\n",
                           h,
                           allocator.Entry(h).position.x,
                           allocator.Entry(h).position.y);
                }
            }
            REQUIRE(allocator.Entry(current).stale == false);

            // Guard against corrupted lists forming a cycle so the test fails
            // instead of hanging.
            if (std::ranges::find(path, current) != path.end())
            {
                printf("Detected a cycle at handle %u (%u, %u)\n",
                       current,
                       allocator.Entry(current).position.x,
                       allocator.Entry(current).position.y);
                for (auto h : path)
                {
                    printf("  -> handle %u (%u, %u)\n",
                           h,
                           allocator.Entry(h).position.x,
                           allocator.Entry(h).position.y);
                }
            }
            REQUIRE(std::ranges::find(path, current) == path.end());

            path.push_back(current);
            auto& entry = allocator.Entry(current);
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

    GIVEN("points along a straight line filling the log")
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
        CHECK(first_path.size() == kTripLogSize);

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
            auto new_handle = trip_log.AddEntry(P(70, 0), 10ms, 100);
            REQUIRE(new_handle.has_value());

            THEN("the first expected removal is pruned")
            {
                auto path = PathToPoints(*new_handle);

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

                    CHECK(std::ranges::find_if(path, [&](auto& p) {
                              return p.point == second_expected_removal;
                          }) == path.end());
                }


                AND_THEN("stale entries are removed")
                {
                    auto h3 = trip_log.AddEntry(P(90, 200), 10ms, 100);
                    REQUIRE(h3.has_value());
                }
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "user reported crash scenario")
{
    constexpr auto kLine = std::array {
        P(852, 298),
        P(852, 297),
        P(852, 297),
        P(849, 292),
        P(847, 287),
        P(844, 282),
        P(839, 281),
        P(834, 284),
        P(829, 286),
        P(824, 289),
        P(819, 291),
        P(814, 294),
        P(809, 296),
        P(804, 299),
        P(799, 301),
        P(794, 304),
        P(789, 306),
    };

    std::optional<LogHandle> handle;
    for (auto entry : kLine)
    {
        auto h = trip_log.AddEntry(entry, 10ms, 100);
        if (h)
        {
            handle = h;
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a long random walk keeps the list consistent")
{
    auto seed = 1234u;
    auto next = [&seed]() {
        seed = seed * 1103515245u + 12345u;
        return static_cast<int>((seed >> 16) % 21) - 10;
    };

    auto x = 1000;
    auto y = 1000;
    for (auto i = 0; i < 2000; i++)
    {
        x += next();
        y += next();

        auto handle = trip_log.AddEntry(P(x, y), 10ms, 100);
        if (!handle)
        {
            continue;
        }

        // WalkBack checks for cycles and stale entries
        auto path = WalkBack(*handle);
        REQUIRE(path.size() <= kTripLogSize + 1);

        for (auto h : path)
        {
            auto& entry = allocator.Entry(h);
            if (entry.predecessor != kInvalidLogHandle)
            {
                REQUIRE(allocator.Entry(entry.predecessor).successor == h);
            }
        }
    }
}

TEST_SUITE_END();
