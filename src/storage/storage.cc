#include "storage.hh"

constexpr auto kMaxSpeedKey = "M";
constexpr auto kBatterySeriesKey = "B";

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

        m_nvm.Commit();
    });

    return std::nullopt;
}
