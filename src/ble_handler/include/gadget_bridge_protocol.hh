#pragma once

#include "application_state.hh"
#include "messages.hh"
#include "post_office.hh"
#include "timer_manager.hh"

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

class GadgetBridgeProtocol
{
public:
    GadgetBridgeProtocol(os::TimerManager& timer_manager,
                         ApplicationState& state,
                         PostOffice<MSG::AllMessages>& post_office,
                         IEventNotifier& notifier);

    // Handle a complete line (without the newline) from Gadgetbridge
    void PushLine(std::string_view line);

    // Set how to send data back to Gadgetbridge
    void SetSender(std::function<void(std::string_view)> sender);

    void Poll();

    // Parse a GB({...}) line into json, or nullopt if it's not a (valid) GB message
    static std::optional<nlohmann::json> ParseLine(std::string_view line);

    // Convert a Gadgetbridge distance ("300 m", "1.4 km", "0.2 mi", "500 ft") to meters,
    // or 0 if it can't be parsed
    static uint32_t DistanceToMeters(std::string_view distance);

    // Round to 5 m (below 100 m) or 10 m, to avoid a flickering display. Non-zero distances
    // stay non-zero, since 0 means no navigation
    static uint32_t RoundDistance(uint32_t meters);

private:
    void HandleNavigationEvent(const nlohmann::json& json);
    void HandleCallEvent(const nlohmann::json& json);
    void HandleNotification(const nlohmann::json& json);
    void HandleNotificationRemoval(const nlohmann::json& json);
    void SendCallControl(std::string_view command);
    void Send(const nlohmann::json& json);

    std::function<void(std::string_view)> m_sender {[](auto) {}};

    os::TimerManager& m_timer_manager;
    ApplicationState& m_state;
    PostOffice<MSG::AllMessages>& m_post_office;

    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
    os::TimerHandle m_navigation_active_timer;
};
