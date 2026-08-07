#include "temperature_monitor.hh"

TemperatureMonitor::TemperatureMonitor(ApplicationState& state)
    : m_state(state)
    , m_state_listener(
          m_state.AttachListener<AS::motor_temperature, AS::controller_temperature, AS::bms_data>(
              GetSemaphore()))
{
}

std::optional<milliseconds>
TemperatureMonitor::OnActivation()
{
    auto rw = m_state.CheckoutReadWrite();

    if (IsOverheated() && rw.Get<AS::overheated>() == false)
    {
        rw.Set<AS::overheated>(true);
        // Clear the overheating condition after a while, if it's not still overheated
        m_overheated_timer = StartTimer(2s, [this]() {
            auto rw = m_state.CheckoutReadWrite();
            auto out = std::optional<milliseconds> {2s};

            if (!IsOverheated())
            {
                // Clear the condition and disable the timer
                rw.Set<AS::overheated>(false);
                out = std::nullopt;
            }

            return out;
        });
    }

    return std::nullopt;
}


bool
TemperatureMonitor::IsOverheated() const
{
    if (m_state.Get<AS::controller_temperature>() > 80)
    {
        return true;
    }

    // TODO: Relevant values here
    if (m_state.Get<AS::motor_temperature>() > 80)
    {
        return true;
    }

    auto bms_data = m_state.Get<AS::bms_data>();
    if (bms_data->valid == false)
    {
        return false;
    }

    return bms_data->bms_temperature > 60 || bms_data->highest_cell_temp > 50;
}
