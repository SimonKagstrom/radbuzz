#pragma once

#include "application_state.hh"
#include "timer_manager.hh"

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

class GadgetBridgeProtocol
{
public:
    GadgetBridgeProtocol(os::TimerManager& timer_manager, ApplicationState& state);

    // Handle a complete line (without the newline) from Gadgetbridge
    void PushLine(std::string_view line);

    // Set how to send data back to Gadgetbridge
    void SetSender(std::function<void(std::string_view)> sender);

    // Parse a GB({...}) line into json, or nullopt if it's not a (valid) GB message
    static std::optional<nlohmann::json> ParseLine(std::string_view line);

private:
    void HandleNavigationEvent(const nlohmann::json& json);
    void Send(const nlohmann::json& json);

    std::function<void(std::string_view)> m_sender {[](auto) {}};

    os::TimerManager& m_timer_manager;
    ApplicationState& m_state;
    os::TimerHandle m_navigation_active_timer;
};
