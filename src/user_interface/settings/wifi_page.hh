#pragma once

#include "application_state.hh"
#include "menu_screen.hh"
#include "messages.hh"
#include "password_entry.hh"
#include "post_office.hh"
#include "wifi_network_list.hh"

#include <lvgl.h>
#include <memory>

// The "Wifi networks" page in the settings menu. Scans when entered, and lists the networks
// from the scan result (see CreateWifiNetworkList). Owned by the settings menu, and destroyed
// before the menu.
class WifiPage
{
public:
    WifiPage(MenuScreen& menu,
             MenuScreen::Page& page,
             lv_indev_t* encoder,
             ApplicationState& state,
             PostOffice<MSG::AllMessages>& post_office);

    // The page is entered (from the on_enter callback)
    void OnEnter();

    // Shown if requested by this page, since the wifi handler also scans periodically
    void OnScanResult(const MSG::wifi_scan_result& result);

    // Refill from the last scan when the connected or known networks have changed, e.g.,
    // after selecting a network, and handle the password entry. Not from an event callback
    // (see MenuScreen::Page::Clear)
    void Update();

private:
    void RequestScan();
    void Rescan();
    void Fill(const MSG::wifi_scan_result& result);
    void AddNetwork(const WifiNetworkListEntry& network);
    void Connect(const WifiNetworkListEntry& network);

    void OpenPasswordEntry(const std::string& ssid);
    // Done, idle or moving
    void UpdatePasswordEntry();
    void ClosePasswordEntry();

    MenuScreen& m_menu;
    MenuScreen::Page& m_page;
    lv_indev_t* m_encoder;
    ApplicationState& m_state;
    PostOffice<MSG::AllMessages>& m_post_office;

    bool m_scan_pending {false};
    // The label of the "Rescan" entry, deleted when the page is cleared
    lv_obj_t* m_rescan_label {nullptr};

    // What the page was last filled with
    std::optional<MSG::wifi_scan_result> m_last_result;
    std::string m_shown_connected_ssid;
    std::vector<WifiSsidNetwork> m_shown_known_networks;

    // For an unknown, secured, network
    std::unique_ptr<PasswordEntry> m_password_entry;
    std::string m_password_ssid;
};
