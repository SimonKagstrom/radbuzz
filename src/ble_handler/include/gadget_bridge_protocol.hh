#pragma once

#include "application_state.hh"
#include "timer_manager.hh"

#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

class GadgetBridgeProtocol
{
public:
    GadgetBridgeProtocol(os::TimerManager& timer_manager, ApplicationState& state);

    // Handle a complete line (without the newline) from Gadgetbridge
    void PushLine(std::string_view line);

    // Parse a GB({...}) line into json, or nullopt if it's not a (valid) GB message
    static std::optional<nlohmann::json> ParseLine(std::string_view line);

private:
    void HandleNavigationEvent(const nlohmann::json& json);

    os::TimerManager& m_timer_manager;
    ApplicationState& m_state;
    os::TimerHandle m_navigation_active_timer;
};
