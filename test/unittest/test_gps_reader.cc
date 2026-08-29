#include "gps_reader.hh"
#include "hal/mock/mock_gps.hh"
#include "test.hh"
#include "thread_fixture.hh"


namespace
{

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().recent_power_distance = 50;

        SetThread(&gps_reader);

        gps_reader.Start("gps_reader");
    }


    ApplicationState state;
    MockGps mock_gps;
    GpsReader gps_reader {state, mock_gps};

    // Some well-known positions
    const GpsPosition kStockholmWgs84 {59.3293, 18.0686};
    const Point stockholm {*Wgs84ToOsmPoint(kStockholmWgs84, kDefaultZoom)};

    const GpsPosition kBraxenWgs84 {59.513855291284244, 17.036614462012853};
    const Point braxen {*Wgs84ToOsmPoint(kBraxenWgs84, kDefaultZoom)};
};

} // namespace


TEST_SUITE_BEGIN("gps_reader");

TEST_CASE_FIXTURE(Fixture, "The GPS state is silent at start")
{
    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kSilent);
}

TEST_CASE_FIXTURE(Fixture, "When started, the GPS reader will wait for data")
{
    REQUIRE_CALL(mock_gps, WaitForData(_)).RETURN(std::nullopt);
    DoRunLoop();
}

TEST_CASE_FIXTURE(Fixture, "When data is received, the GPS state switches to kNoFix")
{
    // Data, but not yet valid
    hal::RawGpsData raw_data {.position = std::nullopt, .heading = 1.0f, .speed = 2.0f};

    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kSilent);

    REQUIRE_CALL(mock_gps, WaitForData(_)).RETURN(raw_data);
    DoRunLoop();

    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kNoFix);
    REQUIRE(*state.Get<AS::pixel_position>() == stockholm);
}

TEST_CASE_FIXTURE(Fixture, "When data dries up, the GPS state switches to kSilent after a timeout")
{
    // Data, but not yet valid
    hal::RawGpsData raw_data {.position = std::nullopt, .heading = 1.0f, .speed = 2.0f};
    REQUIRE_CALL(mock_gps, WaitForData(_)).RETURN(raw_data);
    DoRunLoop();
    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kNoFix);

    WHEN("5 seconds without GPS input has passed")
    {
        ALLOW_CALL(mock_gps, WaitForData(_)).RETURN(std::nullopt);
        AdvanceTimeAndRunLoop(5s);

        THEN("the GPS state switches to kSilent")
        {
            REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kSilent);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "When there is valid GPS data, the pixel position is set")
{
    // Data, but not yet valid
    hal::RawGpsData raw_data {.position = kBraxenWgs84, .heading = 1.0f, .speed = 2.0f};

    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kSilent);

    REQUIRE_CALL(mock_gps, WaitForData(_)).RETURN(raw_data);
    DoRunLoop();

    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kPositionValid);
    REQUIRE(*state.Get<AS::pixel_position>() == braxen);

    AND_WHEN("there is no data for a few seconds")
    {
        ALLOW_CALL(mock_gps, WaitForData(_)).RETURN(std::nullopt);
        AdvanceTimeAndRunLoop(5s);

        THEN("the GPS state switches to kSilent")
        {
            REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kSilent);
        }
    }

    AND_WHEN("there is data, but no position for some time")
    {
        raw_data.position = std::nullopt;
        ALLOW_CALL(mock_gps, WaitForData(_)).RETURN(raw_data);
        AdvanceTimeAndRunLoop(10s);

        THEN("the GPS state switches to kNoFix")
        {
            REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kNoFix);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "When demo mode is active, the GPS state is not updated")
{
    state.CheckoutReadWrite().Set<AS::demo_mode>(true);

    hal::RawGpsData raw_data {.position = kBraxenWgs84, .heading = 1.0f, .speed = 2.0f};
    REQUIRE_CALL(mock_gps, WaitForData(_)).RETURN(raw_data);
    DoRunLoop();

    // No change to the pixel position, or the GPS state (set by the demo app)
    REQUIRE(state.Get<AS::gps_position_valid>() == GpsStatus::kSilent);
    REQUIRE(*state.Get<AS::pixel_position>() == stockholm);
}

TEST_SUITE_END();
