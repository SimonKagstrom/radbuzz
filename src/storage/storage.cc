#include "storage.hh"

#include "split_string.hh"

#include <ranges>

enum class Key
{
    kMaxSpeed,
    kBatterySeries,
    kBatteryAmpHours,
    kWhPerKmForRangeEstimation,
    kSpeedometerType,
    kMaxWatts,
    kRotateMap,
    kForceC6Update,
    kWifiNetworks,

    kValueCount,
};

constexpr auto kKeyToString = std::array {std::pair {
                                              Key::kMaxSpeed,
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
                                              Key::kWifiNetworks,
                                              "W",
                                          }};

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
    , m_state_listener(m_application_state.AttachListener<AS::configuration>(GetSemaphore()))
    , m_state_cache(m_application_state)
{
    auto ps = m_application_state.CheckoutPartialSnapshot<AS::configuration>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    conf.rotate_map = m_nvm.Get<bool>(KeyToString(Key::kRotateMap)).value_or(true);
    conf.max_speed = m_nvm.Get<uint8_t>(KeyToString(Key::kMaxSpeed)).value_or(30);
    conf.battery_cell_series = m_nvm.Get<uint8_t>(KeyToString(Key::kBatterySeries)).value_or(7);
    conf.battery_amp_hours = m_nvm.Get<uint8_t>(KeyToString(Key::kBatteryAmpHours)).value_or(20);
    conf.wh_per_km_for_range_estimation =
        m_nvm.Get<uint8_t>(KeyToString(Key::kWhPerKmForRangeEstimation)).value_or(10);
    conf.speedometer_type =
        static_cast<SpeedometerType>(m_nvm.Get<uint8_t>(KeyToString(Key::kSpeedometerType))
                                         .value_or(std::to_underlying(SpeedometerType::kDigital)));
    conf.max_watts = m_nvm.Get<uint16_t>(KeyToString(Key::kMaxWatts)).value_or(1000);
    conf.force_c6_update = m_nvm.Get<bool>(KeyToString(Key::kForceC6Update)).value_or(false);

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

            auto [ssid, password] = std::tuple(ssid_pass[0], ssid_pass[1]);
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

    if (co.IsChanged<AS::configuration>())
    {
        printf("Configuration changed, writing to NVM...\n");
    }

    co.OnChangedValue<AS::configuration>([this](auto& old_conf, auto& new_conf) {
        if (old_conf.max_speed != new_conf.max_speed)
        {
            m_nvm.Set<uint8_t>(KeyToString(Key::kMaxSpeed), new_conf.max_speed);
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
        if (old_conf.force_c6_update != new_conf.force_c6_update)
        {
            m_nvm.Set<bool>(KeyToString(Key::kForceC6Update), new_conf.force_c6_update);
        }
        if (old_conf.wifi_ssid_data != new_conf.wifi_ssid_data)
        {
            std::string networks;
            for (const auto& network : new_conf.wifi_ssid_data.networks)
            {
                if (!networks.empty())
                {
                    networks += "^";
                }
                networks += network.ssid + "@" + network.password;
            }
            m_nvm.Set<std::string>(KeyToString(Key::kWifiNetworks), networks);
        }
        if (old_conf.rotate_map != new_conf.rotate_map)
        {
            m_nvm.Set<bool>(KeyToString(Key::kRotateMap), new_conf.rotate_map);
        }

        m_nvm.Commit();
    });

    return std::nullopt;
}
