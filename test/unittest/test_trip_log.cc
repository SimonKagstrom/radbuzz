#include "test.hh"
#include "trip_log.hh"

namespace
{

constexpr auto kNumberOfTripLogEntries = 16;

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

    Allocator allocator;
    TripLog<4> trip_log {allocator};
};

} // namespace

TEST_SUITE_BEGIN("trip_log");

TEST_CASE_FIXTURE(Fixture, "the trip log is empty by default")
{
    CHECK(trip_log.GetLastHandle() == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "an entry can be added to the trip log")
{
    auto handle = trip_log.AddEntry({0, 0}, 10ms, 100);

    REQUIRE(handle.has_value());
    CHECK(trip_log.GetLastHandle() == handle);

    auto &entry = allocator.Entry(*handle);
    CHECK(entry.position == Point{0, 0});
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
        auto h0 = trip_log.AddEntry({0, 0}, 10ms, 100);
        REQUIRE(h0.has_value());

        WHEN("a very close entry is added")
        {
            auto h1 = trip_log.AddEntry({4, 4}, 11ms, 100);
            THEN("it's not added")
            {
                CHECK(!h1.has_value());
            }
        }

        WHEN("an entry further away is added")
        {
            // On a straight line
            auto h1 = trip_log.AddEntry({0, 6}, 11ms, 100);
            THEN("it can be added")
            {
                REQUIRE(h1.has_value());
                auto &entry0 = allocator.Entry(*h0);
                auto &entry1 = allocator.Entry(*h1);
                CHECK(entry0.successor == *h1);
                CHECK(entry1.predecessor == *h0);
            }
        }
    }
}

TEST_SUITE_END();
