#include "application_state.hh"
#include "test.hh"

TEST_CASE("The application state has default values")
{
    ApplicationState app_state;

    auto ro = app_state.CheckoutReadonly();
    REQUIRE(ro.Get<AS::wifi_connected>() == false);
    REQUIRE(ro.Get<AS::speed>() == 0);
    REQUIRE(*ro.Get<AS::next_street>() == "");

    auto p = ro.Get<AS::position>();
    REQUIRE(p->position == GpsPosition {0.0f, 0.0f});
    REQUIRE(p->speed == 0.0f);
    REQUIRE(p->heading == 0.0f);
    REQUIRE(p->pixel_position == Point {0, 0});
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
    auto s2 = ro.Get<AS::next_street>();
    REQUIRE(*s == "St Mickelsgatan");
    REQUIRE(*s2 == "St Mickelsgatan");
    REQUIRE(s.get() == s2.get()); // Same pointer
    rw.Set<AS::next_street>("Östra Prinsgatan");
    auto s3 = ro.Get<AS::next_street>();
    REQUIRE(*s == "St Mickelsgatan");
    REQUIRE(*s2 == "St Mickelsgatan");
    REQUIRE(*s3 == "Östra Prinsgatan");

    s = ro.Get<AS::next_street>();
    REQUIRE(*s == "Östra Prinsgatan");
}

TEST_CASE("Non-atomic structs can be written via value")
{
    ApplicationState app_state;
    auto ro = app_state.CheckoutReadonly();
    auto rw = app_state.CheckoutReadWrite();

    GpsData gps_data {
        .position = {59.3293f, 18.0686f},
        .pixel_position = {100, 200},
        .speed = 50.0f,
        .heading = 90.0f,
    };

    rw.Set<AS::position>(gps_data);
    auto p = ro.Get<AS::position>();

    REQUIRE(p->position == GpsPosition {59.3293f, 18.0686f});
    REQUIRE(p->pixel_position == Point {100, 200});
    REQUIRE(p->speed == 50.0f);
    REQUIRE(p->heading == 90.0f);
}

TEST_CASE("Partial snapshot sizes are reasonable")
{
    ApplicationState app_state;
    auto small_snapshot = app_state.CheckoutPartialSnapshot<AS::speed>();
    auto big_snapshot = app_state.CheckoutPartialSnapshot<AS::speed,
                                                          AS::controller_temperature,
                                                          AS::battery_millivolts,
                                                          AS::distance_to_next>();

    REQUIRE(sizeof(small_snapshot) < sizeof(big_snapshot));
    REQUIRE(sizeof(small_snapshot) <= 16); // Reasonable for now
}

TEST_CASE("A partial snapshot keeps a cached state")
{
    ApplicationState app_state;
    auto rw = app_state.CheckoutReadWrite();
    auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed, AS::battery_millivolts>();

    REQUIRE(snapshot.Get<AS::speed>() == 0);

    // In the snapshot
    rw.Set<AS::speed>(10);
    // Not in the snapshot
    rw.Set<AS::controller_temperature>(45);

    REQUIRE(rw.Get<AS::speed>() == 10);
    // Uses the cached copy
    REQUIRE(snapshot.Get<AS::speed>() == 0);
    REQUIRE(rw.Get<AS::controller_temperature>() == 45);

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
    os::binary_semaphore sem {0}, sem_other {0};

    auto listener = app_state.AttachListener<AS::speed, AS::next_street>(sem);
    auto listener_2 = app_state.AttachListener<AS::next_street>(sem_other);
    auto rw = app_state.CheckoutReadWrite();

    WHEN("a non-listened to parameter is changed")
    {
        rw.Set<AS::controller_temperature>(45);

        THEN("the listeners are not notified")
        {
            REQUIRE(sem.try_acquire() == false);
            REQUIRE(sem_other.try_acquire() == false);
        }
    }
    AND_THEN("listened to parameters are notified")
    {
        // Not listened to by the second listener
        rw.Set<AS::speed>(10);
        REQUIRE(sem.try_acquire() == true);
        REQUIRE(sem_other.try_acquire() == false);

        // Listened to by both
        rw.Set<AS::next_street>("St Mickelsgatan");
        REQUIRE(sem.try_acquire() == true);
        REQUIRE(sem_other.try_acquire() == true);
        REQUIRE(sem.try_acquire() == false); // No extra notifications

        AND_WHEN("the same value is set")
        {
            rw.Set<AS::speed>(10);
            rw.Set<AS::next_street>("St Mickelsgatan");
            THEN("there is no notification")
            {
                REQUIRE(sem.try_acquire() == false);
                REQUIRE(sem_other.try_acquire() == false);
            }
        }
        AND_WHEN("the listener is removed")
        {
            listener = nullptr;
            rw.Set<AS::next_street>("Östra Hamngatan");

            THEN("notifications are no longer sent")
            {
                REQUIRE(sem.try_acquire() == false);
            }
            AND_THEN("other listeners are still notified")
            {
                REQUIRE(sem_other.try_acquire() == true);
            }
        }
    }
}

TEST_CASE("There's a finite number of listeners that can be attached")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};

    WHEN("the maximum number of listeners is reached")
    {
        std::vector<std::unique_ptr<ListenerCookie>> listeners;
        std::vector<std::unique_ptr<os::binary_semaphore>> semaphores;
        for (size_t i = 0; i < 255; ++i)
        {
            semaphores.push_back(std::make_unique<os::binary_semaphore>(0));
            auto cur = app_state.AttachListener<AS::speed>(*semaphores[i]);
            REQUIRE(cur);
            listeners.push_back(std::move(cur));
        }

        THEN("no more listeners can be added")
        {
            auto extra_listener = app_state.AttachListener<AS::speed>(sem);
            REQUIRE(extra_listener == nullptr);

            AND_WHEN("a listener is removed")
            {
                listeners[0] = nullptr;

                THEN("a new listener can be added")
                {
                    auto new_listener = app_state.AttachListener<AS::speed>(sem);
                    REQUIRE(new_listener);
                }
            }
        }
    }
}


TEST_CASE("Snapshots affect listeners on destruction")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0}, sem_other {0};

    auto ro = app_state.CheckoutReadonly();
    auto listener = app_state.AttachListener<AS::speed, AS::controller_temperature>(sem);
    auto listener_other = app_state.AttachListener<AS::battery_millivolts>(sem_other);

    {
        auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed>();
        snapshot.Set<AS::speed>(10);
        REQUIRE(sem.try_acquire() == false);
        snapshot.Set<AS::speed>(11);
        REQUIRE(sem.try_acquire() == false);
        REQUIRE(ro.Get<AS::speed>() == 0);

        REQUIRE(sem_other.try_acquire() == false);
    }

    REQUIRE(ro.Get<AS::speed>() == 11);
    REQUIRE(sem.try_acquire() == true);
}

TEST_CASE("the build is OK even with parameters not in the application state")
{
    ApplicationState app_state;
    auto ro = app_state.CheckoutReadonly();

    if constexpr (false)
    {
        // This doesn't exist in the application state, but as a parameter
        ro.Get<AS::only_as_a_parameter>();
    }
}

TEST_CASE("A demo of the application state functionality")
{
    ApplicationState app_state;

    auto rw_task_1 = app_state.CheckoutReadWrite();
    auto rw_task_2 = app_state.CheckoutReadWrite();
    auto ro = app_state.CheckoutReadonly();

    GIVEN("the case when a stable state is not needed")
    {
        THEN("the read-only state is immediately affected by writes")
        {
            rw_task_1.Set<AS::speed>(10);
            REQUIRE(ro.Get<AS::speed>() == 10);
        }
        AND_THEN("also writers can see each other's writes")
        {
            rw_task_1.Set<AS::speed>(20);
            REQUIRE(rw_task_2.Get<AS::speed>() == 20);
        }
    }

    GIVEN("a situation where the local state must be stable")
    {
        // To ensure snapshot destruction
        {
            auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed, AS::motor_temperature>();

            THEN("the snapshot can be used to batch writes")
            {
                snapshot.Set<AS::speed>(30);

                REQUIRE(ro.Get<AS::speed>() == 0);
                REQUIRE(snapshot.Get<AS::speed>() == 30);

                // Note: Compile error since battery_millivolts isn't part of the snapshot
                // snapshot.Get<AS::battery_millivolts>();
            }

            snapshot.Set<AS::speed>(35);
        }
        THEN("the writes are done when the snapshot is destroyed")
        {
            REQUIRE(ro.Get<AS::speed>() == 35);
        }
    }

    GIVEN("the case when multiple values should be changed")
    {
        // To ensure destruction
        {
            auto qw =
                app_state
                    .CheckoutQueuedWriter<AS::controller_temperature, AS::battery_millivolts>();

            qw.Set<AS::controller_temperature>(45);
            qw.Set<AS::battery_millivolts>(3000);

            // Not yet
            THEN("a queued writer can be used, where the values are written on destruction")
            {
                REQUIRE(ro.Get<AS::controller_temperature>() == 0);
                REQUIRE(ro.Get<AS::battery_millivolts>() == 0);
            }
        }
        THEN("the writes are done when the writer is destroyed")
        {
            REQUIRE(ro.Get<AS::controller_temperature>() == 45);
            REQUIRE(ro.Get<AS::battery_millivolts>() == 3000);
        }
    }

    GIVEN("a parameter which doesn't exist in the application state")
    {
        THEN("it can be guarded via constexprs")
        {
            if constexpr (false)
            {
                /*
                 * Undefined symbols for architecture x86_64:
                 *   "AS::storage::OrphanNotFound()", referenced from:
                 *       auto& AS::storage::state::GetRef<AS::only_as_a_parameter>() in test_application_state.cc.o
                 */
                ro.Get<AS::only_as_a_parameter>();
            }
        }
    }
}
