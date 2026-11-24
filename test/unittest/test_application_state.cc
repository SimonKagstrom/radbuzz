#include "application_state.hh"
#include "mock_debug_assert.hh"
#include "semaphore.hh"
#include "test.hh"

namespace
{

class CountingNotifier : public IEventNotifier
{
public:
    unsigned count {0};

private:
    void Notify() final
    {
        count++;
    }

    bool NotifyFromIsr() final
    {
        Notify();
        return false;
    }
};

} // namespace

TEST_SUITE_BEGIN("application_state");

TEST_CASE("The application state has default values")
{
    ApplicationState app_state;

    REQUIRE(app_state.Get<AS::wifi_connected>() == false);
    REQUIRE(app_state.Get<AS::speed>() == 0);
    REQUIRE(*app_state.Get<AS::next_street>() == "");

    auto p = app_state.Get<AS::position>();
    REQUIRE(p->position == GpsPosition {0.0f, 0.0f});
    REQUIRE(p->speed == 0.0f);
    REQUIRE(p->heading == 0.0f);
}

TEST_CASE("Writes are seen immediately in read-only checkouts")
{
    ApplicationState app_state;
    REQUIRE(app_state.Get<AS::speed>() == 0);
    REQUIRE(app_state.Get<AS::speed>() == 0);

    app_state.Set<AS::speed>(10);
    REQUIRE(app_state.Get<AS::speed>() == 10);
    REQUIRE(app_state.Get<AS::speed>() == 10);
}

TEST_CASE("Non-atomic members are kept alive via shared pointers")
{
    ApplicationState app_state;
    app_state.Set<AS::next_street>("St Mickelsgatan");
    auto s = app_state.Get<AS::next_street>();
    auto s2 = app_state.Get<AS::next_street>();
    REQUIRE(*s == "St Mickelsgatan");
    REQUIRE(*s2 == "St Mickelsgatan");
    REQUIRE(s.get() == s2.get()); // Same pointer
    app_state.Set<AS::next_street>("Östra Prinsgatan");
    auto s3 = app_state.Get<AS::next_street>();
    REQUIRE(*s == "St Mickelsgatan");
    REQUIRE(*s2 == "St Mickelsgatan");
    REQUIRE(*s3 == "Östra Prinsgatan");

    s = app_state.Get<AS::next_street>();
    REQUIRE(*s == "Östra Prinsgatan");
}

TEST_CASE("Non-atomic structs can be written via value")
{
    ApplicationState app_state;
    GpsData gps_data {
        .position = {59.3293f, 18.0686f},
        .speed = 50.0f,
        .heading = 90.0f,
    };

    app_state.Set<AS::position>(gps_data);
    auto p = app_state.Get<AS::position>();

    REQUIRE(p->position == GpsPosition {59.3293f, 18.0686f});
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
    auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed, AS::battery_millivolts>();

    REQUIRE(snapshot.Get<AS::speed>() == 0);

    // In the snapshot
    app_state.Set<AS::speed>(10);
    // Not in the snapshot
    app_state.Set<AS::controller_temperature>(45);

    REQUIRE(app_state.Get<AS::speed>() == 10);
    // Uses the cached copy
    REQUIRE(snapshot.Get<AS::speed>() == 0);
    REQUIRE(app_state.Get<AS::controller_temperature>() == 45);

    snapshot.Set<AS::speed>(20);
    REQUIRE(app_state.Get<AS::speed>() == 10);
    REQUIRE(snapshot.Get<AS::speed>() == 20);
}


TEST_CASE("Writes in partial snapshots are written back on destruction")
{
    ApplicationState app_state;
    app_state.Set<AS::next_street>("Tunavägen");

    {
        auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed, AS::next_street>();

        snapshot.Set<AS::speed>(10);
        snapshot.Set<AS::next_street>("St Mickelsgatan");

        REQUIRE(snapshot.Get<AS::speed>() == 10);
        REQUIRE(snapshot.Get<AS::next_street>() == "St Mickelsgatan");

        REQUIRE(app_state.Get<AS::speed>() == 0);
        REQUIRE(*app_state.Get<AS::next_street>() == "Tunavägen");
    }

    REQUIRE(app_state.Get<AS::speed>() == 10);
    REQUIRE(*app_state.Get<AS::next_street>() == "St Mickelsgatan");
}


TEST_CASE("PartialStateCaches can be used to track changes")
{
    ApplicationState app_state;
    auto state_cache =
        ApplicationState::PartialReadOnlyCache<AS::speed, AS::battery_millivolts, AS::next_street>(
            app_state);

    WHEN("a value in the global application state is changed")
    {
        app_state.Set<AS::speed>(10);

        THEN("it's not visible until Pull is called on the partial state cache")
        {
            const auto& co = state_cache.Pull();
            REQUIRE(co.Get<AS::speed>() == 10);
        }

        AND_THEN("it's marked as being changed")
        {
            const auto& co = state_cache.Pull();
            REQUIRE(co.Get<AS::speed>() == 10);
            REQUIRE(co.IsChanged<AS::speed>() == true);

            THEN("other parameters are not marked as changed")
            {
                REQUIRE(co.IsChanged<AS::battery_millivolts>() == false);
            }

            AND_WHEN("it's pulled again")
            {
                const auto& next_co = state_cache.Pull();
                THEN("the change is unmarked again")
                {
                    REQUIRE(next_co.IsChanged<AS::speed>() == false);
                }
                AND_THEN("the value is still correct")
                {
                    REQUIRE(next_co.Get<AS::speed>() == 10);
                }
            }
        }

        AND_WHEN("another change is made")
        {
            app_state.Set<AS::speed>(11);
            const auto& co = state_cache.Pull();
            THEN("also that is visible after pulling")
            {
                REQUIRE(co.Get<AS::speed>() == 11);
            }
            AND_THEN("the change is marked")
            {
                REQUIRE(co.IsChanged<AS::speed>() == true);
                REQUIRE(co.IsChanged<AS::battery_millivolts>() == false);
            }
        }
    }

    WHEN("the user wants to use change callbacks")
    {
        uint8_t cb_speed = 124;
        uint16_t cb_battery = 124;
        std::string cb_next_street = "Kalle Anka";

        auto old_cb_speed = app_state.Get<AS::speed>();
        auto old_cb_battery = app_state.Get<AS::battery_millivolts>();
        auto old_cb_next_street = *app_state.Get<AS::next_street>();

        app_state.Set<AS::speed>(10);
        app_state.Set<AS::next_street>("St Mickelsgatan");

        const auto& co = state_cache.Pull();

        THEN("a callback can be invoked for each change")
        {
            co.OnChanged<AS::speed>([&]() { cb_speed = co.Get<AS::speed>(); })
                .OnChanged<AS::battery_millivolts>(
                    [&]() { cb_battery = co.Get<AS::battery_millivolts>(); })
                .OnChanged<AS::next_street>([&]() { cb_next_street = co.Get<AS::next_street>(); });

            REQUIRE(cb_speed == 10);
            REQUIRE(cb_battery == 124);
            REQUIRE(cb_next_street == "St Mickelsgatan");
        }
        AND_THEN("a callback with the new value can be invoked for each change")
        {
            co.OnNewValue<AS::speed>([&](const auto& new_value) { cb_speed = new_value; })
                .OnNewValue<AS::battery_millivolts>(
                    [&](const auto& new_value) { cb_battery = new_value; })
                .OnNewValue<AS::next_street>(
                    [&](const auto& new_value) { cb_next_street = new_value; });

            REQUIRE(cb_speed == 10);
            REQUIRE(cb_battery == 124);
            REQUIRE(cb_next_street == "St Mickelsgatan");
        }
        AND_THEN("a callback with the both the old and new value can be invoked for each change")
        {
            co.OnChangedValue<AS::speed>([&](const auto& old, const auto& new_value) {
                  old_cb_speed = old;
                  cb_speed = new_value;
              })
                .OnChangedValue<AS::battery_millivolts>(
                    [&](const auto& old, const auto& new_value) {
                        old_cb_battery = old;
                        cb_battery = new_value;
                    })
                .OnChangedValue<AS::next_street>([&](const auto& old, const auto& new_value) {
                    old_cb_next_street = old;
                    cb_next_street = new_value;
                });


            REQUIRE(old_cb_speed == 0);
            REQUIRE(cb_speed == 10);

            REQUIRE(old_cb_battery == 0);
            REQUIRE(cb_battery == 124);

            REQUIRE(old_cb_next_street == "");
            REQUIRE(cb_next_street == "St Mickelsgatan");
        }

        AND_THEN("the change callback functions can be freely mixed")
        {
            co.OnChangedValue<AS::speed>([&](const auto& old, const auto& new_value) {
                  old_cb_speed = old;
                  cb_speed = new_value;
              })
                .OnChanged<AS::battery_millivolts>(
                    [&]() { cb_battery = co.Get<AS::battery_millivolts>(); })
                .OnNewValue<AS::next_street>(
                    [&](const auto& new_value) { cb_next_street = new_value; });


            REQUIRE(old_cb_speed == 0);
            REQUIRE(cb_speed == 10);

            REQUIRE(cb_battery == 124);
            REQUIRE(cb_next_street == "St Mickelsgatan");
        }
    }
}

TEST_CASE("A pulled partial state cache keeps its values until pulled again")
{
    ApplicationState app_state;
    auto state_cache =
        ApplicationState::PartialReadOnlyCache<AS::speed, AS::next_street>(app_state);

    app_state.Set<AS::speed>(10);
    app_state.Set<AS::next_street>("Tunavägen");
    const auto& co = state_cache.Pull();
    REQUIRE(co.Get<AS::next_street>() == "Tunavägen");

    WHEN("the global values are changed after pulling")
    {
        app_state.Set<AS::speed>(11);
        app_state.Set<AS::next_street>("St Mickelsgatan");

        THEN("the checkout still holds the pulled values")
        {
            REQUIRE(co.Get<AS::speed>() == 10);
            REQUIRE(co.Get<AS::next_street>() == "Tunavägen");
            REQUIRE(state_cache.Get<AS::speed>() == 10);
            REQUIRE(state_cache.Get<AS::next_street>() == "Tunavägen");
        }

        AND_WHEN("the cache is pulled again")
        {
            const auto& next_co = state_cache.Pull();

            THEN("the new values are visible, and marked as changed")
            {
                REQUIRE(next_co.Get<AS::speed>() == 11);
                REQUIRE(next_co.Get<AS::next_street>() == "St Mickelsgatan");
                REQUIRE(next_co.IsChanged<AS::speed>());
                REQUIRE(next_co.IsChanged<AS::next_street>());
            }

            AND_WHEN("the values are changed and pulled a third time")
            {
                app_state.Set<AS::speed>(12);
                app_state.Set<AS::next_street>("Östra Prinsgatan");

                REQUIRE(next_co.Get<AS::speed>() == 11);
                REQUIRE(next_co.Get<AS::next_street>() == "St Mickelsgatan");

                const auto& third_co = state_cache.Pull();

                THEN("the reused buffer holds the new values, and the old ones are the previous")
                {
                    REQUIRE(third_co.Get<AS::speed>() == 12);
                    REQUIRE(third_co.Get<AS::next_street>() == "Östra Prinsgatan");

                    third_co
                        .OnChangedValue<AS::speed>([](const auto& old, const auto& new_value) {
                            REQUIRE(old == 11);
                            REQUIRE(new_value == 12);
                        })
                        .OnChangedValue<AS::next_street>(
                            [](const auto& old, const auto& new_value) {
                                REQUIRE(old == "St Mickelsgatan");
                                REQUIRE(new_value == "Östra Prinsgatan");
                            });
                }
            }
        }
    }
}

TEST_CASE("Partial state caches share non-atomic values with the global state")
{
    ApplicationState app_state;
    app_state.Set<AS::next_street>("Tunavägen");

    auto state_cache =
        ApplicationState::PartialReadOnlyCache<AS::speed, AS::next_street>(app_state);

    WHEN("a non-atomic value is unchanged between pulls")
    {
        const auto* before = &state_cache.Get<AS::next_street>();

        app_state.Set<AS::speed>(10);
        const auto& co = state_cache.Pull();

        THEN("the value is not copied")
        {
            REQUIRE(co.IsChanged<AS::speed>());
            REQUIRE(co.IsChanged<AS::next_street>() == false);
            REQUIRE(&co.Get<AS::next_street>() == before);
            REQUIRE(&co.Get<AS::next_street>() == app_state.Get<AS::next_street>().get());
        }
    }

    WHEN("a non-atomic value is changed and then changed back between pulls")
    {
        const auto* before = &state_cache.Get<AS::next_street>();

        app_state.Set<AS::next_street>("St Mickelsgatan");
        app_state.Set<AS::next_street>("Tunavägen");
        const auto& co = state_cache.Pull();

        THEN("the new value is pulled, but not marked as changed")
        {
            REQUIRE(&co.Get<AS::next_street>() != before);
            REQUIRE(&co.Get<AS::next_street>() == app_state.Get<AS::next_street>().get());
            REQUIRE(co.Get<AS::next_street>() == "Tunavägen");
            REQUIRE(co.IsChanged<AS::next_street>() == false);
        }
    }

    WHEN("a reference to a non-atomic value is taken, and the global value changes")
    {
        app_state.Set<AS::next_street>("St Mickelsgatan");
        const auto& old_value = state_cache.Get<AS::next_street>();

        THEN("the reference is unaffected before pulling")
        {
            REQUIRE(old_value == "Tunavägen");
        }

        AND_WHEN("the cache is pulled")
        {
            const auto& co = state_cache.Pull();

            THEN("the new value is passed by reference")
            {
                REQUIRE(co.IsChanged<AS::next_street>());

                co.OnNewValue<AS::next_street>([&app_state](const auto& value) {
                    REQUIRE(&value == app_state.Get<AS::next_street>().get());
                    REQUIRE(value == "St Mickelsgatan");
                });
            }

            AND_THEN("the reference is still valid, and holds the previous value")
            {
                REQUIRE(old_value == "Tunavägen");

                co.OnChangedValue<AS::next_street>(
                    [&old_value](const auto& previous, const auto& value) {
                        REQUIRE(&previous == &old_value);
                        REQUIRE(value == "St Mickelsgatan");
                    });
            }
        }
    }
}

TEST_CASE("A single-parameter partial state cache tracks non-atomic values")
{
    ApplicationState app_state;
    auto state_cache = ApplicationState::PartialReadOnlyCache<AS::position>(app_state);

    REQUIRE(state_cache.Get<AS::position>().speed == 0.0f);

    app_state.Set<AS::position>(GpsData {.position = {59.3293f, 18.0686f}, .speed = 50.0f});
    REQUIRE(state_cache.Get<AS::position>().speed == 0.0f);

    const auto& co = state_cache.Pull();
    REQUIRE(co.IsChanged<AS::position>());
    REQUIRE(co.Get<AS::position>().speed == 50.0f);
    REQUIRE(&co.Get<AS::position>() == app_state.Get<AS::position>().get());

    REQUIRE(state_cache.Pull().IsChanged<AS::position>() == false);
}

TEST_CASE("Listeners can be added to the application state")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0}, sem_other {0};

    auto listener = app_state.AttachListener<AS::speed, AS::next_street>(sem);
    auto listener_2 = app_state.AttachListener<AS::next_street>(sem_other);
    WHEN("a non-listened to parameter is changed")
    {
        app_state.Set<AS::controller_temperature>(45);

        THEN("the listeners are not notified")
        {
            REQUIRE(sem.try_acquire() == false);
            REQUIRE(sem_other.try_acquire() == false);
        }
    }
    AND_THEN("listened to parameters are notified")
    {
        // Not listened to by the second listener
        app_state.Set<AS::speed>(10);
        REQUIRE(sem.try_acquire() == true);
        REQUIRE(sem_other.try_acquire() == false);

        // Listened to by both
        app_state.Set<AS::next_street>("St Mickelsgatan");
        REQUIRE(sem.try_acquire() == true);
        REQUIRE(sem_other.try_acquire() == true);
        REQUIRE(sem.try_acquire() == false); // No extra notifications

        AND_WHEN("the same value is set")
        {
            app_state.Set<AS::speed>(10);
            app_state.Set<AS::next_street>("St Mickelsgatan");
            THEN("there is no notification")
            {
                REQUIRE(sem.try_acquire() == false);
                REQUIRE(sem_other.try_acquire() == false);
            }
        }
        AND_WHEN("the listener is removed")
        {
            listener.Reset();
            app_state.Set<AS::next_street>("Östra Hamngatan");

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


TEST_CASE("A new listener isn't notified of what a removed listener listened to")
{
    ApplicationState app_state;
    os::binary_semaphore sem_removed {0}, sem_new {0};

    auto removed = app_state.AttachListener<AS::speed, AS::next_street>(sem_removed);
    removed.Reset();

    // Takes the place of the removed listener
    auto listener = app_state.AttachListener<AS::next_street>(sem_new);

    WHEN("a parameter only the removed listener listened to is changed")
    {
        app_state.Set<AS::speed>(10);

        THEN("no-one is notified")
        {
            REQUIRE(sem_new.try_acquire() == false);
            REQUIRE(sem_removed.try_acquire() == false);
        }
    }

    WHEN("a parameter both listened to is changed")
    {
        app_state.Set<AS::next_street>("Kungsgatan");

        THEN("only the new listener is notified")
        {
            REQUIRE(sem_new.try_acquire() == true);
            REQUIRE(sem_removed.try_acquire() == false);
        }
    }
}


TEST_CASE("There's a finite number of listeners that can be attached")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};

    WHEN("the maximum number of listeners is reached")
    {
        std::vector<ListenerCookie> listeners;
        std::vector<std::unique_ptr<os::binary_semaphore>> semaphores;
        for (size_t i = 0; i < kMaxApplicationStateListeners; ++i)
        {
            semaphores.push_back(std::make_unique<os::binary_semaphore>(0));
            auto cur = app_state.AttachListener<AS::speed>(*semaphores[i]);
            REQUIRE(cur);
            listeners.push_back(std::move(cur));
        }

        THEN("all can be notified with set")
        {
            app_state.Set<AS::speed>(99);
            for (auto& sem : semaphores)
            {
                CHECK(sem->try_acquire() == true);
            }
        }
        AND_THEN("all can be notified with a queued writer")
        {
            {
                auto queued_writer = app_state.CheckoutQueuedWriter<AS::speed>();
                queued_writer.Set<AS::speed>(99);
            }
            for (auto& sem : semaphores)
            {
                CHECK(sem->try_acquire() == true);
            }
        }

        THEN("no more listeners can be added")
        {
            MockAssertHandler asserts;
            auto r_asserts = NAMED_REQUIRE_CALL(asserts, OnAssert(_, _, _));

            auto extra_listener = app_state.AttachListener<AS::speed>(sem);
            REQUIRE(!extra_listener);

            THEN("the debug assert is triggered")
            {
                r_asserts = nullptr;
            }

            AND_WHEN("a listener is removed")
            {
                listeners[0].Reset();

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

    auto listener = app_state.AttachListener<AS::speed, AS::controller_temperature>(sem);
    auto listener_other = app_state.AttachListener<AS::battery_millivolts>(sem_other);

    {
        auto snapshot = app_state.CheckoutPartialSnapshot<AS::speed>();
        snapshot.Set<AS::speed>(10);
        REQUIRE(sem.try_acquire() == false);
        snapshot.Set<AS::speed>(11);
        REQUIRE(sem.try_acquire() == false);
        REQUIRE(app_state.Get<AS::speed>() == 0);

        REQUIRE(sem_other.try_acquire() == false);
    }

    REQUIRE(app_state.Get<AS::speed>() == 11);
    REQUIRE(sem.try_acquire() == true);
}

TEST_CASE("structs can be changed in parts in partial snapshots")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};

    auto listener = app_state.AttachListener<AS::position>(sem);

    WHEN("a reference is gotten, and a change is made")
    {
        {
            auto snapshot = app_state.CheckoutPartialSnapshot<AS::position>();

            auto& p = snapshot.GetWritableReference<AS::position>();

            REQUIRE(p.speed == 0.0f);
            p.speed = 50.0f;
        }

        THEN("the value is written back, and listeners notified")
        {
            REQUIRE(app_state.Get<AS::position>()->speed == 50.0f);
            REQUIRE(sem.try_acquire() == true);
        }
    }

    WHEN("a reference is gotten, without changes")
    {
        {
            auto snapshot = app_state.CheckoutPartialSnapshot<AS::position>();

            auto& p = snapshot.GetWritableReference<AS::position>();

            REQUIRE(p.speed == 0.0f);
        }

        THEN("nothing is written, and no-one is notified")
        {
            REQUIRE(app_state.Get<AS::position>()->speed == 0.0f);
            REQUIRE(sem.try_acquire() == false);
        }
    }
}

TEST_CASE("a queued writer only writes what it has to")
{
    ApplicationState app_state;

    WHEN("no values are written")
    {
        {
            auto qw = app_state.CheckoutQueuedWriter<AS::speed, AS::battery_millivolts>();
        }

        THEN("nothing changes globally")
        {
            REQUIRE(app_state.Get<AS::speed>() == 0);
            REQUIRE(app_state.Get<AS::battery_millivolts>() == 0);
        }
    }

    WHEN("a single value is written")
    {
        {
            auto qw = app_state.CheckoutQueuedWriter<AS::speed, AS::battery_millivolts>();
            qw.Set<AS::battery_millivolts>(10);
        }

        THEN("only the written value changes globally")
        {
            REQUIRE(app_state.Get<AS::speed>() == 0);
            REQUIRE(app_state.Get<AS::battery_millivolts>() == 10);
        }
    }
}

TEST_CASE("listeners are notified of multiple changes in a batch")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};

    auto listener = app_state.AttachListener<AS::speed, AS::battery_millivolts>(sem);

    {
        auto qw = app_state.CheckoutQueuedWriter<AS::speed, AS::battery_millivolts>();

        qw.Set<AS::speed>(10);
        qw.Set<AS::battery_millivolts>(3000);

        THEN("the writes are not yet visible")
        {
            REQUIRE(app_state.Get<AS::speed>() == 0);
            REQUIRE(app_state.Get<AS::battery_millivolts>() == 0);

            REQUIRE(sem.try_acquire() == false);
        }
    }

    THEN("the writes are done, and the listener notified once")
    {
        REQUIRE(app_state.Get<AS::speed>() == 10);
        REQUIRE(app_state.Get<AS::battery_millivolts>() == 3000);

        // This test will actually always succeed in unittests, as the code executes sequentially
        REQUIRE(sem.try_acquire() == true);
        REQUIRE(sem.try_acquire() == false);
    }
}

TEST_CASE("Non-atomic values are not copied needlessly when written")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};
    auto listener = app_state.AttachListener<AS::copy_counted>(sem);

    CopyCounted::copies = 0;

    WHEN("an equal value is set")
    {
        app_state.Set<AS::copy_counted>(CopyCounted {});

        THEN("it's compared without copies, and not written")
        {
            REQUIRE(CopyCounted::copies == 0);
            REQUIRE(sem.try_acquire() == false);
        }
    }

    WHEN("a new temporary value is set")
    {
        app_state.Set<AS::copy_counted>(CopyCounted {1});

        THEN("it's moved into the global state")
        {
            REQUIRE(CopyCounted::copies == 0);
            REQUIRE(app_state.Get<AS::copy_counted>()->value == 1);
            REQUIRE(sem.try_acquire());
        }
    }

    WHEN("a new value is set from a variable")
    {
        const CopyCounted value {2};
        app_state.Set<AS::copy_counted>(value);

        THEN("it's copied once, into the global state")
        {
            REQUIRE(CopyCounted::copies == 1);
            REQUIRE(app_state.Get<AS::copy_counted>()->value == 2);
            REQUIRE(sem.try_acquire());
        }
    }

    WHEN("a value is changed in a partial snapshot")
    {
        {
            auto snapshot = app_state.CheckoutPartialSnapshot<AS::copy_counted>();
            snapshot.GetWritableReference<AS::copy_counted>().value = 3;
        }

        THEN("only the checkout copies, and the value is moved back")
        {
            REQUIRE(CopyCounted::copies == 1);
            REQUIRE(app_state.Get<AS::copy_counted>()->value == 3);
            REQUIRE(sem.try_acquire());
        }
    }

    WHEN("a reference is taken in a partial snapshot, without changes")
    {
        {
            auto snapshot = app_state.CheckoutPartialSnapshot<AS::copy_counted>();
            snapshot.GetWritableReference<AS::copy_counted>();
        }

        THEN("only the checkout copies, and nothing is written")
        {
            REQUIRE(CopyCounted::copies == 1);
            REQUIRE(sem.try_acquire() == false);
        }
    }

    WHEN("a temporary value is set in a queued writer")
    {
        app_state.CheckoutQueuedWriter<AS::copy_counted>().Set<AS::copy_counted>(CopyCounted {4});

        THEN("it's moved all the way into the global state")
        {
            REQUIRE(CopyCounted::copies == 0);
            REQUIRE(app_state.Get<AS::copy_counted>()->value == 4);
            REQUIRE(sem.try_acquire());
        }
    }
}

TEST_CASE("Written values are converted to the type of the parameter")
{
    ApplicationState app_state;
    os::binary_semaphore sem {0};
    auto listener = app_state.AttachListener<AS::battery_millivolts, AS::position>(sem);

    app_state.Set<AS::battery_millivolts>(48300);
    REQUIRE(sem.try_acquire());

    WHEN("a value of another type is set, which converts to the current value")
    {
        // E.g., the result of a float calculation
        const float millivolts = 48300.25f;
        app_state.Set<AS::battery_millivolts>(millivolts);

        THEN("it's compared after the conversion, so nothing changes")
        {
            REQUIRE(app_state.Get<AS::battery_millivolts>() == 48300);
            REQUIRE(sem.try_acquire() == false);
        }
    }

    WHEN("structs are set from braced initializers")
    {
        app_state.Set<AS::position>({.position = {59.3293f, 18.0686f}, .speed = 50.0f});

        THEN("the value is written")
        {
            REQUIRE(app_state.Get<AS::position>()->speed == 50.0f);
            REQUIRE(sem.try_acquire());
        }

        AND_WHEN("written via a queued writer and a partial snapshot")
        {
            app_state.CheckoutQueuedWriter<AS::position>().Set<AS::position>({.speed = 25.0f});
            REQUIRE(app_state.Get<AS::position>()->speed == 25.0f);

            app_state.CheckoutPartialSnapshot<AS::position>().Set<AS::position>({.speed = 15.0f});
            REQUIRE(app_state.Get<AS::position>()->speed == 15.0f);
        }
    }
}

TEST_CASE("Grouped parameters are independent of each other")
{
    ApplicationState app_state;

    THEN("they have default values")
    {
        REQUIRE(app_state.Get<AS::test_group::number>() == 17);
        REQUIRE(*app_state.Get<AS::test_group::text>() == "default");
        REQUIRE(app_state.Get<AS::test_group::counted>()->value == 0);
        REQUIRE(app_state.Get<AS::test_group::type>() == true);
        REQUIRE(app_state.Get<AS::test_group::nested::flag>() == true);
    }

    WHEN("a parameter has the same name as one outside the group")
    {
        app_state.Set<AS::speed>(10);

        THEN("they are still different parameters")
        {
            REQUIRE(app_state.Get<AS::speed>() == 10);
            REQUIRE(app_state.Get<AS::test_group::speed>() == 70000);

            AND_THEN("they can be used together")
            {
                {
                    auto ps = app_state.CheckoutPartialSnapshot<AS::speed, AS::test_group::speed>();
                    REQUIRE(ps.Get<AS::speed>() == 10);
                    REQUIRE(ps.Get<AS::test_group::speed>() == 70000);

                    ps.Set<AS::speed>(20);
                    ps.Set<AS::test_group::speed>(80000);
                }
                REQUIRE(app_state.Get<AS::speed>() == 20);
                REQUIRE(app_state.Get<AS::test_group::speed>() == 80000);
            }
        }
    }

    WHEN("a parameter is written while another is changed in a snapshot")
    {
        {
            auto ps = app_state.CheckoutPartialSnapshot<AS::test_group::text>();
            ps.GetWritableReference<AS::test_group::text>() += " changed";

            // E.g., by another thread
            app_state.Set<AS::test_group::number>(18);
        }

        THEN("none of the changes are lost")
        {
            REQUIRE(*app_state.Get<AS::test_group::text>() == "default changed");
            REQUIRE(app_state.Get<AS::test_group::number>() == 18);
        }
    }

    WHEN("a parameter is changed")
    {
        os::binary_semaphore sem_number {0}, sem_flag {0};
        auto number_listener = app_state.AttachListener<AS::test_group::number>(sem_number);
        auto flag_listener = app_state.AttachListener<AS::test_group::nested::flag>(sem_flag);

        app_state.Set<AS::test_group::number>(18);

        THEN("only its listeners are notified")
        {
            REQUIRE(sem_number.try_acquire() == true);
            REQUIRE(sem_flag.try_acquire() == false);
        }
    }
}

TEST_CASE("Groups can be used as lists of parameters")
{
    ApplicationState app_state;

    // Without the parameter which isn't in the application state
    static_assert(std::is_same_v<AS::test_group::parameters,
                                 AS::parameter_list<AS::test_group::number,
                                                    AS::test_group::text,
                                                    AS::test_group::counted,
                                                    AS::test_group::speed,
                                                    AS::test_group::type,
                                                    AS::test_group::nested::flag,
                                                    AS::test_group::server::url,
                                                    AS::test_group::server::id>>);

    WHEN("a listener is attached to a group (and a parameter)")
    {
        os::binary_semaphore sem {0};
        auto listener = app_state.AttachListener<AS::test_group, AS::is_moving>(sem);

        THEN("it's notified of changes to all in the group")
        {
            app_state.Set<AS::test_group::number>(18);
            REQUIRE(sem.try_acquire() == true);
            app_state.Set<AS::test_group::text>("other");
            REQUIRE(sem.try_acquire() == true);
            app_state.Set<AS::test_group::nested::flag>(false);
            REQUIRE(sem.try_acquire() == true);
        }
        AND_THEN("it's notified of the other parameter")
        {
            app_state.Set<AS::is_moving>(true);
            REQUIRE(sem.try_acquire() == true);
        }
        AND_THEN("it's not notified of parameters outside the group")
        {
            app_state.Set<AS::speed>(10);
            REQUIRE(sem.try_acquire() == false);
        }
        AND_WHEN("the listener is removed")
        {
            listener.Reset();
            app_state.Set<AS::test_group::number>(18);
            app_state.Set<AS::test_group::nested::flag>(false);

            THEN("it's no longer notified")
            {
                REQUIRE(sem.try_acquire() == false);
            }
        }
    }

    WHEN("a listener is attached to a nested group")
    {
        os::binary_semaphore sem {0};
        auto listener = app_state.AttachListener<AS::test_group::nested>(sem);

        THEN("it's only notified of changes to the nested group")
        {
            app_state.Set<AS::test_group::number>(18);
            REQUIRE(sem.try_acquire() == false);
            app_state.Set<AS::test_group::nested::flag>(false);
            REQUIRE(sem.try_acquire() == true);
        }
    }

    WHEN("a group is used in a read-only cache")
    {
        auto state_cache =
            ApplicationState::PartialReadOnlyCache<AS::speed, AS::test_group>(app_state);

        app_state.Set<AS::test_group::text>("other");
        app_state.Set<AS::test_group::nested::flag>(false);
        const auto& co = state_cache.Pull();

        THEN("changes are seen per parameter")
        {
            REQUIRE(co.IsChanged<AS::test_group::text>());
            REQUIRE(co.IsChanged<AS::test_group::nested::flag>());
            REQUIRE_FALSE(co.IsChanged<AS::test_group::number>());
            REQUIRE_FALSE(co.IsChanged<AS::test_group::speed>());
            REQUIRE_FALSE(co.IsChanged<AS::speed>());

            REQUIRE(co.Get<AS::test_group::text>() == "other");
            REQUIRE(co.Get<AS::test_group::number>() == 17);
        }
    }

    WHEN("a group is written in a batch")
    {
        os::binary_semaphore sem {0};
        auto listener = app_state.AttachListener<AS::test_group>(sem);

        auto require_only_changed_written = [&app_state, &sem]() {
            REQUIRE(app_state.Get<AS::test_group::number>() == 18);
            REQUIRE(app_state.Get<AS::test_group::nested::flag>() == false);
            REQUIRE(*app_state.Get<AS::test_group::text>() == "other");
            REQUIRE(app_state.Get<AS::test_group::speed>() == 70000);
            REQUIRE(sem.try_acquire() == true);
        };

        THEN("a partial snapshot only writes the changed parameters")
        {
            {
                auto ps = app_state.CheckoutPartialSnapshot<AS::test_group>();

                ps.Set<AS::test_group::number>(ps.Get<AS::test_group::number>() + 1);
                ps.Set<AS::test_group::nested::flag>(false);

                // E.g., by another thread
                app_state.Set<AS::test_group::text>("other");
                REQUIRE(sem.try_acquire() == true);
            }
            require_only_changed_written();
        }
        AND_THEN("a queued writer only writes the changed parameters")
        {
            {
                auto qw = app_state.CheckoutQueuedWriter<AS::test_group>();

                qw.Set<AS::test_group::number>(18);
                qw.Set<AS::test_group::nested::flag>(false);

                // E.g., by another thread
                app_state.Set<AS::test_group::text>("other");
                REQUIRE(sem.try_acquire() == true);
            }
            require_only_changed_written();
        }
    }
}

TEST_CASE("the build is OK even with parameters not in the application state")
{
    ApplicationState app_state;

    if constexpr (false)
    {
        // This doesn't exist in the application state, but as a parameter
        app_state.Get<AS::only_as_a_parameter>();
        app_state.Get<AS::test_group::only_as_a_parameter>();
    }
}

TEST_CASE("A demo of the application state functionality")
{
    ApplicationState app_state;

    GIVEN("the case when a stable state is not needed")
    {
        THEN("the state is immediately affected by writes")
        {
            app_state.Set<AS::speed>(10);
            REQUIRE(app_state.Get<AS::speed>() == 10);
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

                REQUIRE(app_state.Get<AS::speed>() == 0);
                REQUIRE(snapshot.Get<AS::speed>() == 30);

                // Note: Compile error since battery_millivolts isn't part of the snapshot
                // snapshot.Get<AS::battery_millivolts>();
            }

            snapshot.Set<AS::speed>(35);
        }
        THEN("the writes are done when the snapshot is destroyed")
        {
            REQUIRE(app_state.Get<AS::speed>() == 35);
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
                REQUIRE(app_state.Get<AS::controller_temperature>() == 0);
                REQUIRE(app_state.Get<AS::battery_millivolts>() == 0);
            }
        }
        THEN("the writes are done when the writer is destroyed")
        {
            REQUIRE(app_state.Get<AS::controller_temperature>() == 45);
            REQUIRE(app_state.Get<AS::battery_millivolts>() == 3000);
        }
    }

    GIVEN("related parameters, in a group of their own")
    {
        // An alternative to a struct parameter, if they should also be usable on their own
        using Server = AS::test_group::server;

        // E.g., in a thread. Listen to (and cache) everything in the group
        CountingNotifier notifier;
        auto listener = app_state.AttachListener<Server>(notifier);
        ApplicationState::PartialReadOnlyCache<Server> state_cache(app_state);

        // E.g., in another thread. Write the parameters together
        auto write_server = [&app_state](const std::string& url, const std::string& id) {
            auto qw = app_state.CheckoutQueuedWriter<Server>();

            qw.Set<Server::url>(url);
            qw.Set<Server::id>(id);
        };

        write_server("https://first", "first");

        THEN("parameters written together give a single notification")
        {
            REQUIRE(notifier.count == 1);
        }

        THEN("the parameters can be read one by one")
        {
            REQUIRE(*app_state.Get<Server::url>() == "https://first");
            REQUIRE(*app_state.Get<Server::id>() == "first");
        }

        THEN("the parameters can be written one by one")
        {
            app_state.Set<Server::id>("second");

            REQUIRE(*app_state.Get<Server::url>() == "https://first");
            REQUIRE(*app_state.Get<Server::id>() == "second");
        }

        THEN("changes are seen per parameter")
        {
            state_cache.Pull();
            app_state.Set<Server::id>("second");

            std::string new_id;
            state_cache.Pull()
                .OnNewValue<Server::url>([](const auto&) { FAIL("The url hasn't changed"); })
                .OnNewValue<Server::id>([&new_id](const auto& id) { new_id = id; });

            REQUIRE(new_id == "second");
        }

        WHEN("the parameters are read one by one during a write")
        {
            auto url = app_state.Get<Server::url>();
            write_server("https://second", "second");
            auto id = app_state.Get<Server::id>();

            THEN("the values might not belong together")
            {
                REQUIRE(*url == "https://first");
                REQUIRE(*id == "second");
            }
        }

        WHEN("the parameters are read via a cache (or snapshot) instead")
        {
            const auto& co = state_cache.Pull();
            auto snapshot = app_state.CheckoutPartialSnapshot<Server>();

            auto cached_url = co.Get<Server::url>();
            auto snapshot_url = snapshot.Get<Server::url>();
            write_server("https://second", "second");

            THEN("the values belong together")
            {
                REQUIRE(cached_url == "https://first");
                REQUIRE(co.Get<Server::id>() == "first");

                REQUIRE(snapshot_url == "https://first");
                REQUIRE(snapshot.Get<Server::id>() == "first");
            }
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
                app_state.Get<AS::only_as_a_parameter>();
            }
        }
    }
}

TEST_SUITE_END();
