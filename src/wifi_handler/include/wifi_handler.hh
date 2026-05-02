#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "hal/i_wifi_client.hh"

class WifiHandler : public os::BaseThread
{
public:
    WifiHandler(ApplicationState& state, Filesystem& filesystem, hal::IWifiClient& wifi_client);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;


    ApplicationState& m_state;
    Filesystem& m_filesystem;
    hal::IWifiClient& m_wifi_client;

    std::unique_ptr<ListenerCookie> m_state_listener;
    std::unique_ptr<ListenerCookie> m_wifi_listener;
};
