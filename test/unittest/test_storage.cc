#include "hal/mock/mock_nvm.hh"
#include "job_pool_thread.hh"
#include "storage.hh"
#include "test.hh"
#include "thread_fixture.hh"

#include <bit>
#include <unordered_map>

namespace
{

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
        // Destroy the old storage before the new one is created
        board = nullptr;
        m_staged = m_committed;

        board = std::make_unique<Board>(nvm);
        SetThread(&board->job_pool);
        board->job_pool.Start("job_pool");
    }

    ApplicationState& State()
    {
        return board->state;
    }

    ConfigurationSettings Configuration()
    {
        return *State().Get<AS::configuration>();
    }

    void SetConfiguration(const ConfigurationSettings& conf)
    {
        State().CheckoutReadWrite().Set<AS::configuration>(conf);
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

    REQUIRE(State().Get<AS::configuration>()->controller_overheat_temperature == 80);
    REQUIRE(State().Get<AS::configuration>()->max_speedometer_speed == 30);
}

TEST_CASE_FIXTURE(Fixture, "the storage thread is awoken on application state changes")
{
    StartBoard();
    // First at startup
    DoRunLoop();

    // Some unrelated change
    auto rw = State().CheckoutReadWrite();
    rw.Set<AS::distance_to_next>("13 meter");
    auto ran = DoRunLoop();
    REQUIRE_FALSE(ran);

    // A change that should wakeup
    auto conf = *State().Get<AS::configuration>();
    conf.max_watts = 9957;
    rw.Set<AS::configuration>(conf);

    ran = DoRunLoop();
    CHECK(ran);
}

TEST_CASE_FIXTURE(Fixture, "the consumption is stored when the moped stops")
{
    StartBoard();

    State().CheckoutReadWrite().Set<AS::is_moving>(true);
    DoRunLoop();
    State().CheckoutReadWrite().Set<AS::wh_consumed>(12.5f);
    State().CheckoutReadWrite().Set<AS::wh_regenerated>(2.25f);

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
        State().CheckoutReadWrite().Set<AS::is_moving>(false);
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

    State().CheckoutReadWrite().Set<AS::demo_mode>(true);
    State().CheckoutReadWrite().Set<AS::is_moving>(true);
    DoRunLoop();

    // Fake values from the demo
    State().CheckoutReadWrite().Set<AS::wh_consumed>(500.0f);
    State().CheckoutReadWrite().Set<AS::wh_regenerated>(100.0f);

    WHEN("the moped stops while in demo mode")
    {
        State().CheckoutReadWrite().Set<AS::is_moving>(false);
        DoRunLoop();
    }

    WHEN("the demo mode is turned off before the moped stops")
    {
        State().CheckoutReadWrite().Set<AS::demo_mode>(false);
        State().CheckoutReadWrite().Set<AS::is_moving>(false);
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
    PresetNvm("f", 1);
    PresetNvm("G", 1);
    PresetNvm("b", 0);
    PresetNvm("x", 1234);
    PresetNvm("y", 5678);
    PresetNvm("0", 81);
    PresetNvm("1", 82);
    PresetNvm("2", 83);
    PresetNvm("3", 84);
    PresetNvm("W", std::string("home@secret^work@pw"));

    StartBoard();
    auto conf = Configuration();

    REQUIRE(conf.max_speedometer_speed == 45);
    REQUIRE(conf.profile == Profile::kMoped45);
    REQUIRE(conf.battery_cell_series == 14);
    REQUIRE(conf.battery_amp_hours == 30);
    REQUIRE(conf.wh_per_km_for_range_estimation == 15);
    REQUIRE(conf.speedometer_type == SpeedometerType::kBoth);
    REQUIRE(conf.max_watts == 4000);
    REQUIRE(conf.recent_power_distance == 250);
    REQUIRE(conf.histogram_mode == HistogramMode::kConsumption);
    REQUIRE(conf.rotate_map == true);
    REQUIRE(conf.force_c6_update == true);
    REQUIRE(conf.show_gps_speed == true);
    REQUIRE(conf.show_speech_bubbles == false);
    REQUIRE(conf.home_position.x == 1234);
    REQUIRE(conf.home_position.y == 5678);
    REQUIRE(conf.controller_overheat_temperature == 81);
    REQUIRE(conf.motor_overheat_temperature == 82);
    REQUIRE(conf.bms_overheat_temperature == 83);
    REQUIRE(conf.cell_overheat_temperature == 84);
    REQUIRE(conf.wifi_ssid_data ==
            WifiSsidData {{WifiSsidNetwork {"home", "secret"}, WifiSsidNetwork {"work", "pw"}}});
}

TEST_CASE_FIXTURE(Fixture, "all settings survive a restart")
{
    StartBoard();

    auto conf = Configuration();
    conf.max_speedometer_speed = 60;
    conf.profile = Profile::kNoLimit;
    conf.battery_cell_series = 20;
    conf.battery_amp_hours = 40;
    conf.wh_per_km_for_range_estimation = 22;
    conf.speedometer_type = SpeedometerType::kAnalog;
    conf.max_watts = 3000;
    conf.recent_power_distance = 75;
    conf.histogram_mode = HistogramMode::kConsumption;
    conf.rotate_map = true;
    conf.force_c6_update = true;
    conf.show_gps_speed = true;
    conf.show_speech_bubbles = false;
    conf.home_position = {-17, 4711, conf.home_position.zoom};
    conf.controller_overheat_temperature = 91;
    conf.motor_overheat_temperature = 92;
    conf.bms_overheat_temperature = 93;
    conf.cell_overheat_temperature = 94;
    conf.wifi_ssid_data = {{{"cafe", "latte"}, {"open", ""}}};
    REQUIRE(conf != Configuration());

    SetConfiguration(conf);
    DoRunLoop();

    StartBoard();
    REQUIRE(Configuration() == conf);
}

TEST_CASE_FIXTURE(Fixture, "broken wifi network entries are skipped")
{
    PresetNvm("W", std::string("home@secret^no separator^^work@pw"));

    StartBoard();

    REQUIRE(Configuration().wifi_ssid_data ==
            WifiSsidData {{WifiSsidNetwork {"home", "secret"}, WifiSsidNetwork {"work", "pw"}}});
}

TEST_CASE_FIXTURE(Fixture, "wifi passwords can be empty or contain the separator")
{
    PresetNvm("W", std::string("open@^cafe@la@tte"));

    StartBoard();

    REQUIRE(Configuration().wifi_ssid_data ==
            WifiSsidData {{WifiSsidNetwork {"open", ""}, WifiSsidNetwork {"cafe", "la@tte"}}});
}

TEST_CASE_FIXTURE(Fixture, "invalid enum values in the NVM are replaced by the defaults")
{
    PresetNvm("6", 77);
    PresetNvm("S", std::to_underlying(SpeedometerType::kValueCount));
    PresetNvm("5", 255);

    StartBoard();
    auto conf = Configuration();

    REQUIRE(conf.profile == Profile::kMoped30);
    REQUIRE(conf.speedometer_type == SpeedometerType::kDigital);
    REQUIRE(conf.histogram_mode == HistogramMode::kPower);
}

TEST_CASE_FIXTURE(Fixture, "changes to configurations are stored to the NVM")
{
    StartBoard();

    auto conf = *State().Get<AS::configuration>();
    conf.max_speedometer_speed = 99;
    State().CheckoutReadWrite().Set<AS::configuration>(conf);
    DoRunLoop();

    WHEN("the board is restarted")
    {
        StartBoard();

        THEN("the configuration is read from NVM")
        {
            REQUIRE(State().Get<AS::configuration>()->max_speedometer_speed == 99);
        }
    }
}

TEST_SUITE_END();
