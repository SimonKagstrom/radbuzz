#include "test.hh"
#include "thread_fixture.hh"
#include "trip_computer.hh"


namespace
{

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(&trip_computer);
    }


    ApplicationState state;
    TripComputer trip_computer {state};
};

} // namespace


TEST_SUITE_BEGIN("trip_computer");

TEST_CASE_FIXTURE(Fixture, "")
{
}

TEST_SUITE_END();
