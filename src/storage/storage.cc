#include "storage.hh"

#include "split_string.hh"

#include <ranges>
#include <string_view>

namespace
{
std::string
TrimAtFirstNul(std::string_view input)
{
    const auto nul_pos = input.find('\0');
    if (nul_pos == std::string_view::npos)
    {
        return std::string(input);
    }

    return std::string(input.substr(0, nul_pos));
}

} // namespace

enum class Key
{
    kMaxSpeedometerSpeed,
    kProfile,
    kBatterySeries,
    kBatteryAmpHours,
    kWhPerKmForRangeEstimation,
    kSpeedometerType,
    kMaxWatts,
    kRecentPowerDistance,
    kHistogramMode,
    kRotateMap,
    kForceC6Update,
    kShowGpsSpeed,
    kWifiNetworks,
    kHomeXPosition,
    kHomeYPosition,
    kWhConsumed,
    kWhRegenerated,
    kSpeechBubbles,
    kControllerOverheatTemperature,
    kMotorOverheatTemperature,
    kBmsOverheatTemperature,
    kCellOverheatTemperature,

    kValueCount,
};

constexpr auto kKeyToString = std::array {
    std::pair {
        Key::kMaxSpeedometerSpeed,
        "M",
    },
    std::pair {
        Key::kBatterySeries,
        "B",
    },
    std::pair {
        Key::kBatteryAmpHours,
        "A",
    },
    std::pair {
        Key::kWhPerKmForRangeEstimation,
        "R",
    },
    std::pair {
        Key::kSpeedometerType,
        "S",
    },
    std::pair {
        Key::kMaxWatts,
        "P",
    },
    std::pair {
        Key::kRotateMap,
        "r",
    },
    std::pair {
        Key::kForceC6Update,
        "f",
    },
    std::pair {
        Key::kShowGpsSpeed,
        "G",
    },
    std::pair {
        Key::kHomeXPosition,
        "x",
    },
    std::pair {
        Key::kHomeYPosition,
        "y",
    },
    std::pair {
        Key::kWhConsumed,
        "C",
    },
    std::pair {Key::kWhRegenerated, "g"},
    std::pair {
        Key::kWifiNetworks,
        "W",
    },
    std::pair {
        Key::kSpeechBubbles,
        "b",
    },
    std::pair {
        Key::kControllerOverheatTemperature,
        "0",
    },
    std::pair {
        Key::kMotorOverheatTemperature,
        "1",
    },
    std::pair {
        Key::kBmsOverheatTemperature,
        "2",
    },
    std::pair {
        Key::kCellOverheatTemperature,
        "3",
    },
    std::pair {
        Key::kRecentPowerDistance,
        "4",
    },
    std::pair {
        Key::kHistogramMode,
        "5",
    },
    std::pair {
        Key::kProfile,
        "6",
    },
};

static_assert(kKeyToString.size() == std::to_underlying(Key::kValueCount));

consteval bool
KeysAreUnique()
{
    auto cpy = kKeyToString;
    std::ranges::sort(cpy, [](const auto& a, const auto& b) { return a.second[0] < b.second[0]; });

    auto [begin, end] = std::ranges::unique(
        cpy, [](const auto& a, const auto& b) { return a.second[0] == b.second[0]; });

    return begin == end;
}
static_assert(KeysAreUnique(), "Keys must be unique in their string representation");

consteval auto
KeyToString(Key key)
{
    return std::find_if(kKeyToString.begin(),
                        kKeyToString.end(),
                        [key](const auto& pair) { return pair.first == key; })
        ->second;
}

Storage::Storage(ApplicationState& application_state, hal::INvm& nvm)
    : m_application_state(application_state)
    , m_nvm(nvm)
    , m_state_listener(
          m_application_state.AttachListener<AS::configuration, AS::is_moving>(GetSemaphore()))
    , m_state_cache(m_application_state)
{
    auto ps =
        m_application_state
            .CheckoutPartialSnapshot<AS::configuration, AS::wh_consumed, AS::wh_regenerated>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    Point home_position {0, 0, kDefaultZoom};
    home_position.x = m_nvm.Get<int32_t>(KeyToString(Key::kHomeXPosition)).value_or(0);
    home_position.y = m_nvm.Get<int32_t>(KeyToString(Key::kHomeYPosition)).value_or(0);

    // Make sure all configuration values are set here, this is where defaults come from
    conf.rotate_map = m_nvm.Get<bool>(KeyToString(Key::kRotateMap)).value_or(false);
    conf.max_speedometer_speed =
        m_nvm.Get<uint8_t>(KeyToString(Key::kMaxSpeedometerSpeed)).value_or(30);
    conf.profile = m_nvm.Get<Profile>(KeyToString(Key::kProfile)).value_or(Profile::kMoped30);
    conf.recent_power_distance =
        m_nvm.Get<uint16_t>(KeyToString(Key::kRecentPowerDistance)).value_or(100);
    conf.battery_cell_series = m_nvm.Get<uint8_t>(KeyToString(Key::kBatterySeries)).value_or(7);
    conf.battery_amp_hours = m_nvm.Get<uint8_t>(KeyToString(Key::kBatteryAmpHours)).value_or(20);
    conf.wh_per_km_for_range_estimation =
        m_nvm.Get<uint8_t>(KeyToString(Key::kWhPerKmForRangeEstimation)).value_or(10);
    conf.show_speech_bubbles = m_nvm.Get<bool>(KeyToString(Key::kSpeechBubbles)).value_or(true);
    conf.speedometer_type =
        static_cast<SpeedometerType>(m_nvm.Get<uint8_t>(KeyToString(Key::kSpeedometerType))
                                         .value_or(std::to_underlying(SpeedometerType::kDigital)));

    conf.histogram_mode =
        static_cast<HistogramMode>(m_nvm.Get<uint8_t>(KeyToString(Key::kHistogramMode))
                                       .value_or(std::to_underlying(HistogramMode::kPower)));
    conf.max_watts = m_nvm.Get<uint16_t>(KeyToString(Key::kMaxWatts)).value_or(1000);
    conf.force_c6_update = m_nvm.Get<bool>(KeyToString(Key::kForceC6Update)).value_or(false);
    conf.show_gps_speed = m_nvm.Get<bool>(KeyToString(Key::kShowGpsSpeed)).value_or(false);
    conf.home_position = home_position;

    conf.bms_overheat_temperature =
        m_nvm.Get<uint8_t>(KeyToString(Key::kBmsOverheatTemperature)).value_or(60);
    conf.motor_overheat_temperature =
        m_nvm.Get<uint8_t>(KeyToString(Key::kMotorOverheatTemperature)).value_or(80);
    conf.controller_overheat_temperature =
        m_nvm.Get<uint8_t>(KeyToString(Key::kControllerOverheatTemperature)).value_or(80);
    conf.cell_overheat_temperature =
        m_nvm.Get<uint8_t>(KeyToString(Key::kCellOverheatTemperature)).value_or(50);
    // ... to here

    // Set the stored consumed/regen values
    ps.Set<AS::wh_consumed>(m_nvm.Get<float>(KeyToString(Key::kWhConsumed)).value_or(0.0f));
    ps.Set<AS::wh_regenerated>(m_nvm.Get<float>(KeyToString(Key::kWhRegenerated)).value_or(0.0f));

    auto networks = m_nvm.Get<std::string>(KeyToString(Key::kWifiNetworks));
    if (networks)
    {
        auto networks_str_list = SplitString(*networks, "^");

        WifiSsidData wifi_data;
        for (const auto& network : networks_str_list)
        {
            auto ssid_pass = SplitString(network, "@");
            if (ssid_pass.size() != 2)
            {
                continue;
            }

            auto ssid = TrimAtFirstNul(ssid_pass[0]);
            auto password = TrimAtFirstNul(ssid_pass[1]);

            wifi_data.networks.push_back({ssid, password});
        }
        conf.wifi_ssid_data = wifi_data;
    }
}

void
Storage::OnStartup()
{
    m_state_cache.Pull();
}

std::optional<milliseconds>
Storage::OnActivation()
{
    auto& co = m_state_cache.Pull();
    auto ro = m_application_state.CheckoutReadonly();

    // Mark as true if demo mode has been active, to not ruin the stored consumption values
    m_tainted_by_demo_mode |= ro.Get<AS::demo_mode>();
    auto do_commit = false;

    co.OnNewValue<AS::is_moving>([this, &ro, &do_commit](auto is_moving) {
        if (!is_moving)
        {
            if (m_tainted_by_demo_mode)
            {
                printf("Demo mode has been active, not saving consumption values to NVM\n");
            }
            else
            {
                m_nvm.Set<float>(KeyToString(Key::kWhConsumed), ro.Get<AS::wh_consumed>());
                m_nvm.Set<float>(KeyToString(Key::kWhRegenerated), ro.Get<AS::wh_regenerated>());

                do_commit = true;
            }
        }
    });

    co.OnChangedValue<AS::configuration>([this, &do_commit](auto& old_conf, auto& new_conf) {
        do_commit = true;

        if (old_conf.max_speedometer_speed != new_conf.max_speedometer_speed)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kMaxSpeedometerSpeed),
                               new_conf.max_speedometer_speed);
        }
        if (old_conf.profile != new_conf.profile)
        {
            m_nvm.Set<Profile>(KeyToString(Key::kProfile), new_conf.profile);
        }
        if (old_conf.battery_cell_series != new_conf.battery_cell_series)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kBatterySeries), new_conf.battery_cell_series);
        }
        if (old_conf.battery_amp_hours != new_conf.battery_amp_hours)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kBatteryAmpHours), new_conf.battery_amp_hours);
        }
        if (old_conf.wh_per_km_for_range_estimation != new_conf.wh_per_km_for_range_estimation)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kWhPerKmForRangeEstimation),
                               new_conf.wh_per_km_for_range_estimation);
        }
        if (old_conf.max_watts != new_conf.max_watts)
        {
            m_nvm.Set<uint16_t>(KeyToString(Key::kMaxWatts), new_conf.max_watts);
        }
        if (old_conf.speedometer_type != new_conf.speedometer_type)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kSpeedometerType),
                               static_cast<uint8_t>(new_conf.speedometer_type));
        }
        if (old_conf.histogram_mode != new_conf.histogram_mode)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kHistogramMode),
                               static_cast<uint8_t>(new_conf.histogram_mode));
        }
        if (old_conf.force_c6_update != new_conf.force_c6_update)
        {
            m_nvm.Set<bool>(KeyToString(Key::kForceC6Update), new_conf.force_c6_update);
        }
        if (old_conf.wifi_ssid_data != new_conf.wifi_ssid_data)
        {
            std::string networks;
            for (const auto& network : new_conf.wifi_ssid_data.networks)
            {
                const auto ssid = TrimAtFirstNul(network.ssid);
                const auto password = TrimAtFirstNul(network.password);
                if (!networks.empty())
                {
                    networks += "^";
                }
                networks += ssid + "@" + password;
            }
            m_nvm.Set<std::string>(KeyToString(Key::kWifiNetworks), networks);
        }
        if (old_conf.rotate_map != new_conf.rotate_map)
        {
            m_nvm.Set<bool>(KeyToString(Key::kRotateMap), new_conf.rotate_map);
        }
        if (old_conf.recent_power_distance != new_conf.recent_power_distance)
        {
            m_nvm.Set<uint16_t>(KeyToString(Key::kRecentPowerDistance),
                                new_conf.recent_power_distance);
        }
        if (old_conf.show_gps_speed != new_conf.show_gps_speed)
        {
            m_nvm.Set<bool>(KeyToString(Key::kShowGpsSpeed), new_conf.show_gps_speed);
        }
        if (old_conf.show_speech_bubbles != new_conf.show_speech_bubbles)
        {
            m_nvm.Set<bool>(KeyToString(Key::kSpeechBubbles), new_conf.show_speech_bubbles);
        }
        if (old_conf.home_position != new_conf.home_position)
        {
            m_nvm.Set<int32_t>(KeyToString(Key::kHomeXPosition), new_conf.home_position.x);
            m_nvm.Set<int32_t>(KeyToString(Key::kHomeYPosition), new_conf.home_position.y);
        }

        // Temperature limits
        if (old_conf.bms_overheat_temperature != new_conf.bms_overheat_temperature)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kBmsOverheatTemperature),
                               new_conf.bms_overheat_temperature);
        }
        if (old_conf.motor_overheat_temperature != new_conf.motor_overheat_temperature)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kMotorOverheatTemperature),
                               new_conf.motor_overheat_temperature);
        }
        if (old_conf.controller_overheat_temperature != new_conf.controller_overheat_temperature)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kControllerOverheatTemperature),
                               new_conf.controller_overheat_temperature);
        }
        if (old_conf.cell_overheat_temperature != new_conf.cell_overheat_temperature)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kCellOverheatTemperature),
                               new_conf.cell_overheat_temperature);
        }
    });

    if (do_commit)
    {
        printf("Writing to NVM...\n");
        m_nvm.Commit();
    }

    return std::nullopt;
}
