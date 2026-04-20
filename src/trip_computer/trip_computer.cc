#include "trip_computer.hh"

#include <numeric>

namespace
{

constexpr auto kMillivoltSocTable = std::array {std::pair<uint16_t, uint8_t> {3270, 0},
                                                std::pair<uint16_t, uint8_t> {3390, 10},
                                                std::pair<uint16_t, uint8_t> {3510, 20},
                                                std::pair<uint16_t, uint8_t> {3610, 30},
                                                std::pair<uint16_t, uint8_t> {3680, 40},
                                                std::pair<uint16_t, uint8_t> {3740, 50},
                                                std::pair<uint16_t, uint8_t> {3820, 60},
                                                std::pair<uint16_t, uint8_t> {3930, 70},
                                                std::pair<uint16_t, uint8_t> {4050, 80},
                                                std::pair<uint16_t, uint8_t> {4130, 90},
                                                std::pair<uint16_t, uint8_t> {4200, 100}};

uint8_t
InterpolateSoc(uint16_t millivolts)
{
    if (millivolts <= kMillivoltSocTable.front().first)
    {
        return kMillivoltSocTable.front().second;
    }
    if (millivolts >= kMillivoltSocTable.back().first)
    {
        return kMillivoltSocTable.back().second;
    }

    for (size_t i = 1; i < kMillivoltSocTable.size(); ++i)
    {
        const auto& [mv_low, soc_low] = kMillivoltSocTable[i - 1];
        const auto& [mv_high, soc_high] = kMillivoltSocTable[i];

        if (millivolts < mv_high)
        {
            float ratio = static_cast<float>(millivolts - mv_low) / (mv_high - mv_low);
            return static_cast<uint8_t>(soc_low + ratio * (soc_high - soc_low));
        }
    }

    // Should never reach here
    return 0;
}
} // namespace

TripComputer::TripComputer(ApplicationState& app_state)
    : m_state(app_state)
    , m_state_listener(m_state.AttachListener<AS::battery_series>(GetSemaphore()))
{
    m_soc_timer = StartTimer(250ms, [this]() {
        auto mv = m_state.CheckoutReadonly().Get<AS::battery_millivolts>();
        if (mv != 0)
        {
            UpdateSoc(mv);
        }
        return 250ms;
    });
}

void
TripComputer::UpdateSoc(uint16_t millivolts)
{
    if (m_millivolt_history.empty())
    {
        m_state.CheckoutReadWrite().Set<AS::battery_soc>(InterpolateSoc(millivolts));
    }

    auto ro = m_state.CheckoutReadonly();

    if (ro.Get<AS::current_power_w>() > 300)
    {
        // Limit SOC updates on high power
        return;
    }

    m_millivolt_history.push(millivolts);
    if (m_millivolt_history.full())
    {
        auto soc = InterpolateSoc(
            std::accumulate(m_millivolt_history.begin(), m_millivolt_history.end(), 0u) /
            m_millivolt_history.size());

        m_state.CheckoutReadWrite().Set<AS::battery_soc>(soc);
        // Discard the oldest
        m_millivolt_history.pop();
    }
}

std::optional<milliseconds>
TripComputer::OnActivation()
{
    return 1s;
}
