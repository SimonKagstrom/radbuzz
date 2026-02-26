#include "hal/mock/mock_stepper_motor.hh"
#include "speedometer_handler.hh"
#include "test.hh"
#include "thread_fixture.hh"

namespace
{

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(&speedo);
    }


    ApplicationState state;
    MockStepperMotor motor;
    SpeedometerHandler speedo {motor, state, 6000};
};

class StartedFixture : public Fixture
{
public:
    StartedFixture()
    {
        REQUIRE_CALL(motor, Step(_));

        speedo.Start("speedometer");
    }
};

} // namespace


TEST_CASE_FIXTURE(Fixture, "the speedometer will move to zero once started")
{
    REQUIRE_CALL(motor, Step(-6000));

    speedo.Start("speedometer");
}


TEST_CASE_FIXTURE(StartedFixture, "the speedometer will only listen to speed changes")
{
    auto rw = state.CheckoutReadWrite();

    WHEN("non-speed-related application state changes")
    {
        auto r_no_step = NAMED_FORBID_CALL(motor, Step(_));

        rw.Set<AS::battery_millivolts>(3000);
        rw.Set<AS::controller_temperature>(45);
        rw.Set<AS::next_street>("Tunavägen");
        rw.Set<AS::distance_travelled>(100);

        DoRunLoop();

        THEN("the stepper motor is untouched")
        {
            r_no_step = nullptr;
        }
    }

    WHEN("the speed is changed")
    {
        auto r_step = NAMED_REQUIRE_CALL(motor, Step(_));

        rw.Set<AS::speed>(100);

        DoRunLoop();

        THEN("the stepper motor is moved")
        {
            r_step = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(StartedFixture, "the stepper motor position is scaled with speed")
{
    int pos = 0;

    ALLOW_CALL(motor, Step(_)).LR_SIDE_EFFECT(pos += _1);
    auto do_set_speed = [this](uint8_t speed) {
        auto rw = state.CheckoutReadWrite();
        rw.Set<AS::speed>(speed);
        DoRunLoop();
    };

    do_set_speed(30);
    REQUIRE(pos == 3000);

    do_set_speed(60);
    REQUIRE(pos == 6000);

    // Cap at 60km/h
    do_set_speed(100);
    REQUIRE(pos == 6000);

    do_set_speed(10);
    REQUIRE(pos == 1000);

    do_set_speed(1);
    REQUIRE(pos == 100);

    do_set_speed(5);
    REQUIRE(pos == 500);
}
