#include "storage.hh"

#include "split_string.hh"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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

template <auto Member>
struct Setting
{
    using Type =
        std::remove_cvref_t<decltype(std::declval<ConfigurationSettings>().*Member)>;
    static constexpr auto member = Member;

    const char* key;
    Type default_value;
};

// The keys must never change, since they are what is stored in the NVM
constexpr auto kSettings = std::tuple {
    Setting<&ConfigurationSettings::max_speedometer_speed> {"M", 30},
    Setting<&ConfigurationSettings::profile> {"6", Profile::kMoped30},
    Setting<&ConfigurationSettings::battery_cell_series> {"B", 7},
    Setting<&ConfigurationSettings::battery_amp_hours> {"A", 20},
    Setting<&ConfigurationSettings::wh_per_km_for_range_estimation> {"R", 10},
    Setting<&ConfigurationSettings::speedometer_type> {"S", SpeedometerType::kDigital},
    Setting<&ConfigurationSettings::max_watts> {"P", 1000},
    Setting<&ConfigurationSettings::recent_power_distance> {"4", 100},
    Setting<&ConfigurationSettings::histogram_mode> {"5", HistogramMode::kPower},
    Setting<&ConfigurationSettings::rotate_map> {"r", false},
    Setting<&ConfigurationSettings::force_c6_update> {"f", false},
    Setting<&ConfigurationSettings::show_gps_speed> {"G", false},
    Setting<&ConfigurationSettings::show_speech_bubbles> {"b", true},
    Setting<&ConfigurationSettings::controller_overheat_temperature> {"0", 80},
    Setting<&ConfigurationSettings::motor_overheat_temperature> {"1", 80},
    Setting<&ConfigurationSettings::bms_overheat_temperature> {"2", 60},
    Setting<&ConfigurationSettings::cell_overheat_temperature> {"3", 50},
};

// Not plain configuration values, handled separately
constexpr auto kHomeXPositionKey = "x";
constexpr auto kHomeYPositionKey = "y";
constexpr auto kWifiNetworksKey = "W";
constexpr auto kWhConsumedKey = "C";
constexpr auto kWhRegeneratedKey = "g";

template <typename Function>
constexpr void
ForEachSetting(Function&& function)
{
    std::apply([&function](const auto&... setting) { (function(setting), ...); }, kSettings);
}

consteval bool
KeysAreUnique()
{
    std::vector<std::string_view> keys {
        kHomeXPositionKey, kHomeYPositionKey, kWifiNetworksKey, kWhConsumedKey, kWhRegeneratedKey};
    ForEachSetting([&keys](const auto& setting) { keys.push_back(setting.key); });

    std::ranges::sort(keys);
    return std::ranges::adjacent_find(keys) == keys.end();
}
static_assert(KeysAreUnique(), "NVM keys must be unique");

template <typename T>
bool
IsValid(T value)
{
    if constexpr (std::is_enum_v<T>)
    {
        // E.g., a corrupt NVM
        return std::to_underlying(value) < std::to_underlying(T::kValueCount);
    }

    return true;
}

Storage::Storage(ApplicationState& application_state, hal::INvm& nvm)
    : m_application_state(application_state)
    , m_nvm(nvm)
    , m_state_listener(
          m_application_state.AttachListener<AS::configuration, AS::is_moving>(*this))
    , m_state_cache(m_application_state)
{
    auto ps =
        m_application_state
            .CheckoutPartialSnapshot<AS::configuration, AS::wh_consumed, AS::wh_regenerated>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    ForEachSetting([this, &conf](const auto& setting) {
        using Type = typename std::remove_cvref_t<decltype(setting)>::Type;

        auto value = m_nvm.Get<Type>(setting.key).value_or(setting.default_value);
        conf.*setting.member = IsValid(value) ? value : setting.default_value;
    });

    conf.home_position = {m_nvm.Get<int32_t>(kHomeXPositionKey).value_or(0),
                          m_nvm.Get<int32_t>(kHomeYPositionKey).value_or(0),
                          kDefaultZoom};

    // Set the stored consumed/regen values
    ps.Set<AS::wh_consumed>(m_nvm.Get<float>(kWhConsumedKey).value_or(0.0f));
    ps.Set<AS::wh_regenerated>(m_nvm.Get<float>(kWhRegeneratedKey).value_or(0.0f));

    auto networks = m_nvm.Get<std::string>(kWifiNetworksKey);
    if (networks)
    {
        auto networks_str_list = SplitString(*networks, "^");

        WifiSsidData wifi_data;
        for (const auto& network : networks_str_list)
        {
            // Not SplitString, since the password can be empty (or contain '@')
            auto separator = network.find('@');
            if (separator == std::string::npos)
            {
                continue;
            }

            auto ssid = TrimAtFirstNul(std::string_view(network).substr(0, separator));
            auto password = TrimAtFirstNul(std::string_view(network).substr(separator + 1));

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
                m_nvm.Set<float>(kWhConsumedKey, ro.Get<AS::wh_consumed>());
                m_nvm.Set<float>(kWhRegeneratedKey, ro.Get<AS::wh_regenerated>());

                do_commit = true;
            }
        }
    });

    co.OnChangedValue<AS::configuration>([this, &do_commit](auto& old_conf, auto& new_conf) {
        do_commit = true;

        ForEachSetting([this, &old_conf, &new_conf](const auto& setting) {
            if (old_conf.*setting.member != new_conf.*setting.member)
            {
                m_nvm.Set(setting.key, new_conf.*setting.member);
            }
        });

        if (old_conf.home_position != new_conf.home_position)
        {
            m_nvm.Set<int32_t>(kHomeXPositionKey, new_conf.home_position.x);
            m_nvm.Set<int32_t>(kHomeYPositionKey, new_conf.home_position.y);
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
            m_nvm.Set<std::string>(kWifiNetworksKey, networks);
        }
    });

    if (do_commit)
    {
        printf("Writing to NVM...\n");
        m_nvm.Commit();
    }

    return std::nullopt;
}
