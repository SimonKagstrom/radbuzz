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

private:
    void HandleNavigationEvent(const nlohmann::json& json);
    void HandleCallEvent(const nlohmann::json& json);
    void SendCallControl(std::string_view command);
    void Send(const nlohmann::json& json);

    std::function<void(std::string_view)> m_sender {[](auto) {}};

    os::TimerManager& m_timer_manager;
    ApplicationState& m_state;
    PostOffice<MSG::AllMessages>& m_post_office;

    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
    os::TimerHandle m_navigation_active_timer;
};
