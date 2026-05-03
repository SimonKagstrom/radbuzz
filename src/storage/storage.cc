#include "storage.hh"

#include "split_string.hh"

constexpr auto kMaxSpeedKey = "M";
constexpr auto kBatterySeriesKey = "B";
constexpr auto kWifiNetworks = "W";
constexpr auto kBatteryAmpHoursKey = "A";

Storage::Storage(ApplicationState& application_state, hal::INvm& nvm)
    : m_application_state(application_state)
    , m_nvm(nvm)
    , m_state_listener(m_application_state.AttachListener<AS::configuration>(GetSemaphore()))
    , m_state_cache(m_application_state)
{
}

void
Storage::OnStartup()
{
    auto ps = m_application_state.CheckoutPartialSnapshot<AS::configuration>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    conf.max_speed = m_nvm.Get<uint8_t>(kMaxSpeedKey).value_or(30);
    conf.battery_cell_series = m_nvm.Get<uint8_t>(kBatterySeriesKey).value_or(7);
    conf.battery_amp_hours = m_nvm.Get<uint8_t>(kBatteryAmpHoursKey).value_or(20);

    auto networks = m_nvm.Get<std::string>(kWifiNetworks);
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

    m_state_cache.Pull();
}

std::optional<milliseconds>
Storage::OnActivation()
{
    auto& co = m_state_cache.Pull();

    co.OnChangedValue<AS::configuration>([this](auto& old_conf, auto& new_conf) {
        if (old_conf.max_speed != new_conf.max_speed)
        {
            m_nvm.Set<uint8_t>(kMaxSpeedKey, new_conf.max_speed);
        }
        if (old_conf.battery_cell_series != new_conf.battery_cell_series)
        {
            m_nvm.Set<uint8_t>(kBatterySeriesKey, new_conf.battery_cell_series);
        }
        if (old_conf.battery_amp_hours != new_conf.battery_amp_hours)
        {
            m_nvm.Set<uint8_t>(kBatteryAmpHoursKey, new_conf.battery_amp_hours);
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
            m_nvm.Set<std::string>(kWifiNetworks, networks);
        }

        m_nvm.Commit();
    });

    return std::nullopt;
}
