#include "hal/mock/mock_nvm.hh"
#include "job_pool_thread.hh"
#include "storage.hh"
#include "test.hh"
#include "thread_fixture.hh"

#include <bit>
#include <tuple>
#include <unordered_map>

namespace
{

using Conf = AS::configuration;

template <class... T>
bool
AllDiffer(const std::tuple<T...>& a, const std::tuple<T...>& b)
{
    return [&a, &b]<size_t... I>(std::index_sequence<I...>) {
        return ((std::get<I>(a) != std::get<I>(b)) && ...);
    }(std::index_sequence_for<T...> {});
}

// What is restarted together with the board, i.e., everything except the NVM
struct Board
{
    explicit Board(hal::INvm& nvm)
    {
        auto s = std::make_unique<Storage>(state, nvm);
        storage = s.get();

        job_pool.AttachPooledThread(std::move(s));
    }

    // Declared first, since the storage (owned by the pool) listens to it
    ApplicationState state;
    JobPoolThread job_pool;
    Storage* storage;
};

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        m_expectations.push_back(NAMED_ALLOW_CALL(nvm, Commit()).SIDE_EFFECT(DoCommit()));
        m_expectations.push_back(NAMED_ALLOW_CALL(nvm, EraseAll()).SIDE_EFFECT(DoEraseAll()));
        m_expectations.push_back(NAMED_ALLOW_CALL(nvm, EraseKey(_)).SIDE_EFFECT(DoEraseKey(_1)));
        m_expectations.push_back(NAMED_ALLOW_CALL(nvm, GetUint32_t(_)).RETURN(DoGetUint32_t(_1)));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(nvm, SetUint32_t(_, _)).SIDE_EFFECT(DoSetUint32_t(_1, _2)));
        m_expectations.push_back(NAMED_ALLOW_CALL(nvm, GetString(_)).RETURN(DoGetString(_1)));
        m_expectations.push_back(
            NAMED_ALLOW_CALL(nvm, SetString(_, _)).SIDE_EFFECT(DoSetString(_1, _2)));
    }

    // Start, or restart, the board. Uncommitted NVM changes are lost, like on the target
    void StartBoard()
    {
        CreateBoard();
        StartThreads();
    }

    // The first half of StartBoard(): The storage is created, but its thread isn't started
    void CreateBoard()
    {
        // Destroy the old storage before the new one is created
        board = nullptr;
        m_staged = m_committed;

        board = std::make_unique<Board>(nvm);
    }

    void StartThreads()
    {
        SetThread(&board->job_pool);
        board->job_pool.Start("job_pool");
    }

    ApplicationState& State()
    {
        return board->state;
    }

    template <typename S>
    auto Value()
    {
        if constexpr (S::IsAtomic())
        {
            return State().Get<S>();
        }
        else
        {
            return *State().Get<S>();
        }
    }

    // Everything in the groups which are stored in the NVM, as a tuple
    auto StoredSettings()
    {
        auto values = [this]<class... S>(AS::parameter_list<S...>) {
            return std::tuple {Value<S>()...};
        };

        return std::tuple_cat(values(AS::configuration::parameters {}),
                              values(AS::traccar::parameters {}));
    }

    // As if written (and committed) by an earlier boot
    void PresetNvm(const std::string& key, uint32_t value)
    {
        m_staged.numbers[key] = value;
        m_committed.numbers[key] = value;
    }

    void PresetNvm(const std::string& key, const std::string& value)
    {
        m_staged.strings[key] = value;
        m_committed.strings[key] = value;
    }

    unsigned CommitCount() const
    {
        return m_commit_count;
    }

    MockNvm nvm;
    std::unique_ptr<Board> board;

private:
    struct Contents
    {
        std::unordered_map<std::string, uint32_t> numbers;
        std::unordered_map<std::string, std::string> strings;
    };

    void DoCommit()
    {
        m_committed = m_staged;
        m_commit_count++;
    }

    void DoEraseAll()
    {
        m_staged = {};
    }

    void DoEraseKey(const char* key)
    {
        m_staged.numbers.erase(key);
        m_staged.strings.erase(key);
    }

    std::optional<uint32_t> DoGetUint32_t(const char* key)
    {
        if (auto it = m_staged.numbers.find(key); it != m_staged.numbers.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void DoSetUint32_t(const char* key, uint32_t value)
    {
        m_staged.numbers[key] = value;
    }

    std::optional<std::string> DoGetString(const char* key)
    {
        if (auto it = m_staged.strings.find(key); it != m_staged.strings.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void DoSetString(const char* key, const std::string_view value)
    {
        m_staged.strings[key] = value;
    }

    Contents m_staged;
    Contents m_committed;
    unsigned m_commit_count {0};

    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

} // namespace


TEST_SUITE_BEGIN("storage");

TEST_CASE_FIXTURE(Fixture, "the storage thread sets default values if the NVM is empty")
{
    StartBoard();

    REQUIRE(State().Get<Conf::controller_overheat_temperature>() == 80);
    REQUIRE(State().Get<Conf::max_speedometer_speed>() == 30);
    REQUIRE(State().Get<Conf::show_clock>() == true);
    REQUIRE(State().Get<Conf::tile_style>() == TileStyle::kCycle);
    REQUIRE(State().Get<Conf::brightness_mode>() == BrightnessMode::kAuto);
    REQUIRE(*State().Get<AS::traccar::configuration>() == TraccarSettings {});
}

TEST_CASE_FIXTURE(Fixture, "the storage thread is awoken on application state changes")
{
    StartBoard();
    // First at startup
    DoRunLoop();

    // Some unrelated change
    State().Set<AS::distance_to_next>(13);
    auto ran = DoRunLoop();
    REQUIRE_FALSE(ran);

    // A change that should wakeup
    State().Set<Conf::max_watts>(9957);

    ran = DoRunLoop();
    CHECK(ran);
}

TEST_CASE_FIXTURE(Fixture, "the consumption is stored when the moped stops")
{
    StartBoard();

    State().Set<AS::is_moving>(true);
    DoRunLoop();
    State().Set<AS::wh_consumed>(12.5f);
    State().Set<AS::wh_regenerated>(2.25f);

    WHEN("the moped is still moving")
    {
        auto commits_before = CommitCount();
        DoRunLoop();

        THEN("nothing is written")
        {
            REQUIRE(CommitCount() == commits_before);
        }
    }

    WHEN("the moped stops")
    {
        State().Set<AS::is_moving>(false);
        DoRunLoop();

        AND_WHEN("the board is restarted")
        {
            StartBoard();

            THEN("the consumption values are restored")
            {
                REQUIRE(State().Get<AS::wh_consumed>() == 12.5f);
                REQUIRE(State().Get<AS::wh_regenerated>() == 2.25f);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the demo mode makes the storage thread ignore Wh storage")
{
    PresetNvm("C", std::bit_cast<uint32_t>(5.0f));
    PresetNvm("g", std::bit_cast<uint32_t>(1.0f));
    StartBoard();
    REQUIRE(State().Get<AS::wh_consumed>() == 5.0f);

    State().Set<AS::demo_mode>(true);
    State().Set<AS::is_moving>(true);
    DoRunLoop();

    // Fake values from the demo
    State().Set<AS::wh_consumed>(500.0f);
    State().Set<AS::wh_regenerated>(100.0f);

    WHEN("the moped stops while in demo mode")
    {
        State().Set<AS::is_moving>(false);
        DoRunLoop();
    }

    WHEN("the demo mode is turned off before the moped stops")
    {
        State().Set<AS::demo_mode>(false);
        State().Set<AS::is_moving>(false);
        DoRunLoop();
    }

    // For both of the above
    StartBoard();
    THEN("the demo values are not stored")
    {
        REQUIRE(State().Get<AS::wh_consumed>() == 5.0f);
        REQUIRE(State().Get<AS::wh_regenerated>() == 1.0f);
    }
}

TEST_CASE_FIXTURE(Fixture, "settings stored by an earlier firmware are read")
{
    // The NVM keys must not change, or devices in the field lose their settings
    PresetNvm("M", 45);
    PresetNvm("6", std::to_underlying(Profile::kMoped45));
    PresetNvm("B", 14);
    PresetNvm("A", 30);
    PresetNvm("R", 15);
    PresetNvm("S", std::to_underlying(SpeedometerType::kBoth));
    PresetNvm("P", 4000);
    PresetNvm("4", 250);
    PresetNvm("5", std::to_underlying(HistogramMode::kConsumption));
    PresetNvm("r", 1);
    PresetNvm("T", std::to_underlying(TileStyle::kLandscape));
    PresetNvm("f", 1);
    PresetNvm("G", 1);
    PresetNvm("c", 0);
    PresetNvm("b", 0);
    PresetNvm("x", 1234);
    PresetNvm("y", 5678);
    PresetNvm("0", 81);
    PresetNvm("1", 82);
    PresetNvm("2", 83);
    PresetNvm("3", 84);
    PresetNvm("L", std::to_underlying(BrightnessMode::kMedium));
    PresetNvm("W", std::string("home@secret^work@pw"));

    StartBoard();

    REQUIRE(State().Get<Conf::max_speedometer_speed>() == 45);
    REQUIRE(State().Get<Conf::profile>() == Profile::kMoped45);
    REQUIRE(State().Get<Conf::battery_cell_series>() == 14);
    REQUIRE(State().Get<Conf::battery_amp_hours>() == 30);
    REQUIRE(State().Get<Conf::wh_per_km_for_range_estimation>() == 15);
    REQUIRE(State().Get<Conf::speedometer_type>() == SpeedometerType::kBoth);
    REQUIRE(State().Get<Conf::max_watts>() == 4000);
    REQUIRE(State().Get<Conf::recent_power_distance>() == 250);
    REQUIRE(State().Get<Conf::histogram_mode>() == HistogramMode::kConsumption);
    REQUIRE(State().Get<Conf::rotate_map>() == true);
    REQUIRE(State().Get<Conf::tile_style>() == TileStyle::kLandscape);
    REQUIRE(State().Get<Conf::show_gps_speed>() == true);
    REQUIRE(State().Get<Conf::show_clock>() == false);
    REQUIRE(State().Get<Conf::show_speech_bubbles>() == false);
    REQUIRE(State().Get<Conf::home_position>()->x == 1234);
    REQUIRE(State().Get<Conf::home_position>()->y == 5678);
    REQUIRE(State().Get<Conf::controller_overheat_temperature>() == 81);
    REQUIRE(State().Get<Conf::motor_overheat_temperature>() == 82);
    REQUIRE(State().Get<Conf::bms_overheat_temperature>() == 83);
    REQUIRE(State().Get<Conf::cell_overheat_temperature>() == 84);
    REQUIRE(State().Get<Conf::brightness_mode>() == BrightnessMode::kMedium);
    REQUIRE(*State().Get<Conf::wifi_ssid_data>() ==
            WifiSsidData {{WifiSsidNetwork {"home", "secret"}, WifiSsidNetwork {"work", "pw"}}});
}

TEST_CASE_FIXTURE(Fixture, "all settings survive a restart")
{
    StartBoard();

    const auto defaults = StoredSettings();

    State().Set<Conf::max_speedometer_speed>(60);
    State().Set<Conf::profile>(Profile::kNoLimit);
    State().Set<Conf::battery_cell_series>(20);
    State().Set<Conf::battery_amp_hours>(40);
    State().Set<Conf::wh_per_km_for_range_estimation>(22);
    State().Set<Conf::speedometer_type>(SpeedometerType::kAnalog);
    State().Set<Conf::max_watts>(3000);
    State().Set<Conf::recent_power_distance>(75);
    State().Set<Conf::histogram_mode>(HistogramMode::kConsumption);
    State().Set<Conf::rotate_map>(true);
    State().Set<Conf::tile_style>(TileStyle::kMobileAtlas);
    State().Set<Conf::show_gps_speed>(true);
    State().Set<Conf::show_clock>(false);
    State().Set<Conf::show_speech_bubbles>(false);
    State().Set<Conf::home_position>({-17, 4711, kDefaultZoom});
    State().Set<Conf::controller_overheat_temperature>(91);
    State().Set<Conf::motor_overheat_temperature>(92);
    State().Set<Conf::bms_overheat_temperature>(93);
    State().Set<Conf::cell_overheat_temperature>(94);
    State().Set<Conf::wifi_ssid_data>({{{"cafe", "latte"}, {"open", ""}}});
    // All 32 bits are used
    State().Set<Conf::ssid_file_hash>(0xdeadbeef);
    State().Set<AS::traccar::configuration>(
        {.url = "https://pi.tail.ts.net", .id = "k3v9x2qmw7rt", .demo_id = "p8d4"});
    State().Set<AS::traccar::configuration_file_hash>(0xfeedface);
    State().Set<Conf::brightness_mode>(BrightnessMode::kLow);

    // New settings must be added above
    const auto changed = StoredSettings();
    REQUIRE(AllDiffer(changed, defaults));

    DoRunLoop();

    StartBoard();
    REQUIRE(StoredSettings() == changed);
}

TEST_CASE_FIXTURE(Fixture, "only changed settings are written to the NVM")
{
    StartBoard();
    DoRunLoop();

    auto r_no_other = NAMED_FORBID_CALL(nvm, SetUint32_t(_, _));
    auto r_no_string = NAMED_FORBID_CALL(nvm, SetString(_, _));
    // The last matching expectation is used
    auto r_write = NAMED_REQUIRE_CALL(nvm, SetUint32_t(_, 3000u)).WITH(std::string_view(_1) == "P");
    const auto commits_before = CommitCount();

    State().Set<Conf::max_watts>(3000);
    DoRunLoop();

    THEN("a single value is written and committed")
    {
        r_write = nullptr;
        REQUIRE(CommitCount() == commits_before + 1);
    }
}

TEST_CASE_FIXTURE(Fixture, "settings changed before the storage thread starts are stored")
{
    const TraccarSettings traccar {.url = "https://pi.tail.ts.net", .id = "k3v9", .demo_id = "p8"};
    const WifiSsidData networks {{{"cafe", "latte"}}};

    PresetNvm("M", 60);
    CreateBoard();

    // E.g., imported from the SD card by another thread, which was started first
    State().Set<AS::traccar::configuration>(traccar);
    State().Set<AS::traccar::configuration_file_hash>(0xfeedface);
    State().Set<Conf::wifi_ssid_data>(networks);
    State().Set<AS::timezone>(60);

    StartThreads();
    DoRunLoop();

    WHEN("the board is restarted")
    {
        StartBoard();

        THEN("the changes are kept, together with what was stored before")
        {
            CHECK(*State().Get<AS::traccar::configuration>() == traccar);
            CHECK(State().Get<AS::traccar::configuration_file_hash>() == 0xfeedface);
            CHECK(*State().Get<Conf::wifi_ssid_data>() == networks);
            CHECK(State().Get<AS::timezone>() == 60);
            CHECK(State().Get<Conf::max_speedometer_speed>() == 60);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "nothing is written to the NVM at startup, unless changed")
{
    PresetNvm("M", 60);
    PresetNvm("Z", 60);
    PresetNvm("W", std::string("home@secret"));
    PresetNvm("U", std::string("https://pi.tail.ts.net"));
    // Replaced by the default
    PresetNvm("6", 77);

    StartBoard();
    DoRunLoop();

    CHECK(CommitCount() == 0);
}

TEST_CASE_FIXTURE(Fixture, "broken wifi network entries are skipped")
{
    PresetNvm("W", std::string("home@secret^no separator^^work@pw"));

    StartBoard();

    REQUIRE(*State().Get<Conf::wifi_ssid_data>() ==
            WifiSsidData {{WifiSsidNetwork {"home", "secret"}, WifiSsidNetwork {"work", "pw"}}});
}

TEST_CASE_FIXTURE(Fixture, "wifi passwords can be empty or contain the separator")
{
    PresetNvm("W", std::string("open@^cafe@la@tte"));

    StartBoard();

    REQUIRE(*State().Get<Conf::wifi_ssid_data>() ==
            WifiSsidData {{WifiSsidNetwork {"open", ""}, WifiSsidNetwork {"cafe", "la@tte"}}});
}

TEST_CASE_FIXTURE(Fixture, "invalid enum values in the NVM are replaced by the defaults")
{
    PresetNvm("6", 77);
    PresetNvm("S", std::to_underlying(SpeedometerType::kValueCount));
    PresetNvm("5", 255);
    PresetNvm("T", std::to_underlying(TileStyle::kValueCount));
    PresetNvm("L", std::to_underlying(BrightnessMode::kValueCount));

    StartBoard();

    REQUIRE(State().Get<Conf::profile>() == Profile::kMoped30);
    REQUIRE(State().Get<Conf::speedometer_type>() == SpeedometerType::kDigital);
    REQUIRE(State().Get<Conf::histogram_mode>() == HistogramMode::kPower);
    REQUIRE(State().Get<Conf::tile_style>() == TileStyle::kCycle);
    REQUIRE(State().Get<Conf::brightness_mode>() == BrightnessMode::kAuto);
}

TEST_CASE_FIXTURE(Fixture, "changes to configurations are stored to the NVM")
{
    StartBoard();

    State().Set<Conf::max_speedometer_speed>(99);
    DoRunLoop();

    WHEN("the board is restarted")
    {
        StartBoard();

        THEN("the configuration is read from NVM")
        {
            REQUIRE(State().Get<Conf::max_speedometer_speed>() == 99);
        }
    }
}


TEST_CASE_FIXTURE(Fixture, "the timezone is stored to the NVM")
{
    StartBoard();
    REQUIRE(State().Get<AS::timezone>() == 0);

    WHEN("the timezone is set")
    {
        State().Set<AS::timezone>(-210);
        DoRunLoop();

        AND_WHEN("the board is restarted")
        {
            StartBoard();

            THEN("the timezone is restored")
            {
                REQUIRE(State().Get<AS::timezone>() == -210);
            }
        }

        AND_WHEN("it's set to the same value again (e.g., on the next phone connect)")
        {
            auto commits_before = CommitCount();
            State().Set<AS::timezone>(-210);
            DoRunLoop();

            THEN("nothing is written")
            {
                REQUIRE(CommitCount() == commits_before);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the timezone stored by an earlier firmware is read")
{
    // The key must not change. Stored in the low 16 bits
    PresetNvm("Z", static_cast<uint16_t>(-60));
    StartBoard();

    REQUIRE(State().Get<AS::timezone>() == -60);
}

TEST_CASE_FIXTURE(Fixture, "invalid timezones in the NVM are replaced by UTC")
{
    // Outside of UTC-12..UTC+14, e.g., a corrupt NVM
    WHEN("it's too large")
    {
        PresetNvm("Z", 15 * 60);
        StartBoard();

        THEN("UTC is used")
        {
            REQUIRE(State().Get<AS::timezone>() == 0);
        }
    }

    WHEN("it's too small")
    {
        PresetNvm("Z", static_cast<uint16_t>(-13 * 60));
        StartBoard();

        THEN("UTC is used")
        {
            REQUIRE(State().Get<AS::timezone>() == 0);
        }
    }
}

TEST_SUITE_END();
