#include "application_state.hh"
#include "test.hh"

TEST_CASE("The application state has default values")
{
    ApplicationState app_state;

    auto ro = app_state.CheckoutReadonly();
    REQUIRE(ro.Get<AS::wifi_connected>() == false);
    REQUIRE(ro.Get<AS::speed>() == 0);
    REQUIRE(*ro.Get<AS::next_street>() == "");
}

TEST_CASE("Writes are seen immediately in read-only checkouts")
{
    ApplicationState app_state;
    auto ro = app_state.CheckoutReadonly();
    auto rw = app_state.CheckoutReadWrite();

    REQUIRE(ro.Get<AS::speed>() == 0);
    REQUIRE(rw.Get<AS::speed>() == 0);

    rw.Set<AS::speed>(10);
    REQUIRE(ro.Get<AS::speed>() == 10);
    REQUIRE(rw.Get<AS::speed>() == 10);
}

TEST_CASE("Non-atomic members are kept alive via shared pointers")
{
    ApplicationState app_state;
    auto ro = app_state.CheckoutReadonly();
    auto rw = app_state.CheckoutReadWrite();

    rw.Set<AS::next_street>("St Mickelsgatan");
    auto s = ro.Get<AS::next_street>();
    REQUIRE(*s == "St Mickelsgatan");
    rw.Set<AS::next_street>("Östra Prinsgatan");
    auto s2 = ro.Get<AS::next_street>();
    REQUIRE(*s == "St Mickelsgatan");
    REQUIRE(*s2 == "Östra Prinsgatan");
}


TEST_CASE("A partial snapshot keeps a cached state")
{
    ApplicationState app_state;
    auto rw = app_state.CheckoutReadWrite();
    auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed, AS::battery_millivolts>();

    REQUIRE(snapshot.Get<AS::speed>() == 0);
    REQUIRE(snapshot.Get<AS::controller_temperature>() == 0);

    // In the snapshot
    rw.Set<AS::speed>(10);
    // Not in the snapshot - forwarded
    rw.Set<AS::controller_temperature>(45);

    REQUIRE(rw.Get<AS::speed>() == 10);
    REQUIRE(snapshot.Get<AS::speed>() == 0);
    REQUIRE(snapshot.Get<AS::controller_temperature>() == 45);

    snapshot.Set<AS::speed>(20);
    REQUIRE(rw.Get<AS::speed>() == 10);
    REQUIRE(snapshot.Get<AS::speed>() == 20);
}


TEST_CASE("Writes in partial snapshots are written back on destruction")
{
    ApplicationState app_state;
    auto ro = app_state.CheckoutReadonly();
    app_state.CheckoutReadWrite().Set<AS::next_street>("Tunavägen");

    {
        auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed, AS::next_street>();

        snapshot.Set<AS::speed>(10);
        snapshot.Set<AS::next_street>("St Mickelsgatan");

        REQUIRE(snapshot.Get<AS::speed>() == 10);
        REQUIRE(snapshot.Get<AS::next_street>() == "St Mickelsgatan");

        REQUIRE(ro.Get<AS::speed>() == 0);
        REQUIRE(*ro.Get<AS::next_street>() == "Tunavägen");
    }

    REQUIRE(ro.Get<AS::speed>() == 10);
    REQUIRE(*ro.Get<AS::next_street>() == "St Mickelsgatan");
}


TEST_CASE("Listeners can be added to the application state")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};

    auto listener = app_state.AttachListener<AS::speed, AS::next_street>(sem);

    WHEN("a non-listened to parameter is changed")
    {
        app_state.CheckoutReadWrite().Set<AS::controller_temperature>(45);

        THEN("the listener is not notified")
        {
            REQUIRE(sem.try_acquire() == false);
        }
    }
    AND_THEN("listened to parameters are notified")
    {
        app_state.CheckoutReadWrite().Set<AS::speed>(10);
        REQUIRE(sem.try_acquire() == true);

        app_state.CheckoutReadWrite().Set<AS::next_street>("St Mickelsgatan");
        REQUIRE(sem.try_acquire() == true);
    }
}


TEST_CASE("Snapshots affect listeners on destruction")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};

    auto ro = app_state.CheckoutReadonly();
    auto listener = app_state.AttachListener<AS::speed, AS::controller_temperature>(sem);

    {
        auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed>();
        snapshot.Set<AS::speed>(10);
        REQUIRE(sem.try_acquire() == false);
        snapshot.Set<AS::speed>(11);
        REQUIRE(sem.try_acquire() == false);
        REQUIRE(ro.Get<AS::speed>() == 0);
    }

    REQUIRE(ro.Get<AS::speed>() == 11);
    REQUIRE(sem.try_acquire() == true);
}
