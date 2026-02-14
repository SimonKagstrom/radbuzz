#include "application_state.hh"
#include "test.hh"
#if 0
namespace XX
{

// Generated
struct speed
{
    uint8_t speed;

    template<typename T>
    auto &GetRef();

    template<>
    auto &GetRef<struct speed>()
    {
        return speed;
    }

    consteval bool IsAtomic()
    {
        return true;
    }
};

struct battery_millivolts
{
    uint16_t battery_millivolts;

    template<typename T>
    auto &GetRef();

    template<>
    auto &GetRef<struct battery_millivolts>()
    {
        return battery_millivolts;
    }

    consteval bool IsAtomic()
    {
        return true;
    }
};

struct next_street
{
    std::shared_ptr<std::string> next_street;

    template<typename T>
    auto &GetRef();

    template<>
    auto &GetRef<struct next_street>()
    {
        return next_street;
    }

    static auto DefaultValue()
    {
        return std::make_shared<std::string>("");
    }
};

template<typename T>
struct IsAtomicTrait
{
    static constexpr bool value = true;
};

template<>
struct IsAtomicTrait<struct speed>
{
    static constexpr bool value = true;
};

template<>
struct IsAtomicTrait<struct next_street>
{
    static constexpr bool value = false;
};

template<typename T>
consteval bool IsAtomic()
{
    return IsAtomicTrait<T>::value;
}


template<typename T>
auto DefaultValue();

template<>
auto DefaultValue<struct next_street>()
{
    return std::make_shared<std::string>("St Mickelsgatan");
};

template<>
auto DefaultValue<speed>()
{
    return 10;
};

template<>
auto DefaultValue<battery_millivolts>()
{
    return 0;
};


namespace storage
{

struct state
{
    uint8_t speed;
    uint16_t battery_millivolts;
    std::shared_ptr<std::string> next_street;

    template<typename T>
    auto &GetRef();

    template<>
    auto &GetRef<struct speed>()
    {
        return speed;
    }

    template<>
    auto &GetRef<struct next_street>()
    {
        return next_street;
    }

    template<>
    auto &GetRef<struct battery_millivolts>()
    {
        return battery_millivolts;
    }

    void SetupDefaultValues()
    {
        speed = DefaultValue<struct speed>();
        battery_millivolts = DefaultValue<struct battery_millivolts>();
        next_street = DefaultValue<struct next_street>();
    }
};

template<class... T>
struct partial_state : public T...
{
    template<typename S>
    auto &GetRef()
    {
        return static_cast<S&>(*this).template GetRef<S>();
    }
};

};

}

class AppState
{
public:
    AppState()
    {
        m_state.SetupDefaultValues();
    }

    template<typename T>
    auto Get()
    {
        if constexpr (XX::IsAtomic<T>())
        {
            return m_state.GetRef<T>();
        }
        else
        {
            auto ptr = m_state.GetRef<T>();

            using element_type = typename decltype(ptr)::element_type;
            return std::make_shared<const element_type>(*ptr);
        }
    }

private:
    XX::storage::state m_state;
};

template <class... T>
class PartialSnapshot
{
public:
    explicit PartialSnapshot(AppState& app_state) : m_app_state(app_state)
    {
        (void)std::initializer_list<int>{
            (m_state.template GetRef<T>() = m_app_state.Get<T>(), 0)...
        };
    }

    template<typename S>
    auto Get()
    {
        // If S not in T, return the global state
        if constexpr (!std::disjunction_v<std::is_same<S, T>...>)
        {
            return m_app_state.Get<S>();
        }

        if constexpr (XX::IsAtomic<S>())
        {
            return m_state.template GetRef<S>();
        }
        else
        {
            auto ptr = m_state.template GetRef<S>();
            using element_type = typename decltype(ptr)::element_type;
            return std::make_shared<const element_type>(*ptr);
        }
    }
private:
    AppState& m_app_state;
    XX::storage::partial_state<T...> m_state;
};
#endif

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

    rw.Set<AS::speed>(10);
    REQUIRE(rw.Get<AS::speed>() == 10);
    REQUIRE(snapshot.Get<AS::speed>() == 0);

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
