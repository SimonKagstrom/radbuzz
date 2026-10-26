#include "wifi_page.hh"

#include <radbuzz_font_16.h>
#include <radbuzz_wifi_symbols_22.h>
#include <string>

namespace
{

// Material Symbols, see radbuzz_wifi_symbols_22
constexpr auto kCheckIcon = "\xEE\x97\x8A";
constexpr auto kLockIcon = "\xEE\xA2\x97";
constexpr auto kSignal0Icon = "\xEF\x82\xB0";
constexpr auto kSignal1Icon = "\xEE\xAF\xA4";
constexpr auto kSignal2Icon = "\xEE\xAF\x96";
constexpr auto kSignal3Icon = "\xEE\xAF\xA1";
constexpr auto kSignal4Icon = "\xEE\x87\x98";

const char*
SignalIcon(int8_t rssi)
{
    if (rssi >= -55)
    {
        return kSignal4Icon;
    }
    if (rssi >= -67)
    {
        return kSignal3Icon;
    }
    if (rssi >= -78)
    {
        return kSignal2Icon;
    }
    if (rssi >= -89)
    {
        return kSignal1Icon;
    }

    return kSignal0Icon;
}

} // namespace

WifiPage::WifiPage(MenuScreen& menu,
                   MenuScreen::Page& page,
                   lv_indev_t* encoder,
                   ApplicationState& state,
                   PostOffice<MSG::AllMessages>& post_office)
    : m_menu(menu)
    , m_page(page)
    , m_encoder(encoder)
    , m_state(state)
    , m_post_office(post_office)
{
}

void
WifiPage::OnEnter()
{
    // The networks from the last visit are probably outdated
    m_page.Clear();
    m_rescan_label = nullptr;
    m_page.AddEntry("Scanning…", []() { /* NOP */ });

    RequestScan();
}

void
WifiPage::OnScanResult(const MSG::wifi_scan_result& result)
{
    if (!m_scan_pending)
    {
        return;
    }

    m_scan_pending = false;
    m_last_result = result;
    Fill(result);
}

void
WifiPage::Update()
{
    UpdatePasswordEntry();

    // Nothing shown yet, or about to be refilled anyway
    if (!m_last_result || m_scan_pending)
    {
        return;
    }

    if (*m_state.Get<AS::connected_wifi_ssid>() != m_shown_connected_ssid ||
        m_state.Get<AS::configuration>()->wifi_ssid_data.networks != m_shown_known_networks)
    {
        Fill(*m_last_result);
    }
}

void
WifiPage::RequestScan()
{
    m_scan_pending = true;
    m_post_office.Send<MSG::start_wifi_scan>();
}

void
WifiPage::Rescan()
{
    if (m_scan_pending)
    {
        return;
    }

    // Not cleared here, since this is called from the event callback of an entry on the page
    lv_label_set_text(m_rescan_label, "Scanning…");
    RequestScan();
}

void
WifiPage::Fill(const MSG::wifi_scan_result& result)
{
    m_shown_connected_ssid = *m_state.Get<AS::connected_wifi_ssid>();
    m_shown_known_networks = m_state.Get<AS::configuration>()->wifi_ssid_data.networks;

    const auto networks =
        CreateWifiNetworkList(result.networks, m_shown_known_networks, m_shown_connected_ssid);

    m_page.Clear();
    if (networks.empty())
    {
        m_page.AddEntry("No networks found", []() { /* NOP */ });
    }
    for (const auto& network : networks)
    {
        AddNetwork(network);
    }

    m_page.AddSeparator();
    auto rescan_row = m_page.AddEntry("Rescan", [this]() { Rescan(); });
    m_rescan_label = lv_obj_get_child(rescan_row, 0);
}

void
WifiPage::AddNetwork(const WifiNetworkListEntry& network)
{
    auto row = m_page.AddEntry(network.ssid, [this, network]() { Connect(network); });

    // Long SSIDs (up to 32 bytes) shouldn't push the icons out
    auto ssid_label = lv_obj_get_child(row, 0);
    lv_obj_set_width(ssid_label, 1);
    lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_DOT);

    if (network.password && !network.connected)
    {
        auto saved_label = lv_label_create(row);
        lv_obj_set_style_text_font(saved_label, &radbuzz_font_16, LV_PART_MAIN);
        lv_label_set_text(saved_label, "Saved");
    }

    std::string icons;
    if (network.connected)
    {
        icons += kCheckIcon;
    }
    if (network.type == MSG::WifiType::kSecured)
    {
        icons += kLockIcon;
    }
    if (network.rssi)
    {
        icons += SignalIcon(*network.rssi);
    }

    if (!icons.empty())
    {
        auto icons_label = lv_label_create(row);
        lv_obj_set_style_text_font(icons_label, &radbuzz_wifi_symbols_22, LV_PART_MAIN);
        // The font has no space
        lv_obj_set_style_text_letter_space(icons_label, 6, LV_PART_MAIN);
        lv_label_set_text(icons_label, icons.c_str());
    }
}

void
WifiPage::Connect(const WifiNetworkListEntry& network)
{
    if (network.connected)
    {
        return;
    }

    if (network.password)
    {
        m_post_office.Send(MSG::wifi_connect {network.ssid, *network.password});
    }
    else if (network.type == MSG::WifiType::kOpen)
    {
        m_post_office.Send(MSG::wifi_connect {network.ssid, ""});
    }
    else
    {
        // Secured (or unknown type, so probably secured)
        OpenPasswordEntry(network.ssid);
    }
}

void
WifiPage::OpenPasswordEntry(const std::string& ssid)
{
    if (m_password_entry)
    {
        printf("Wifi page: Password entry already open\n");
        return;
    }
    // Not while riding
    if (m_state.Get<AS::is_moving>())
    {
        printf("Wifi page: No password entry while moving\n");
        return;
    }

    printf("Wifi page: Password entry for %s\n", ssid.c_str());
    m_password_ssid = ssid;
    m_password_entry = std::make_unique<PasswordEntry>(m_encoder, ssid);
    // The password entry has its own (longer) timeout
    m_menu.SetExitTimerEnabled(false);
}

void
WifiPage::UpdatePasswordEntry()
{
    if (!m_password_entry)
    {
        return;
    }

    switch (m_password_entry->GetResult())
    {
    case PasswordEntry::Result::kAccepted:
        // Stored (also if wrong) and connected by the wifi handler. The page is refilled
        // when connected
        m_post_office.Send(MSG::wifi_connect {m_password_ssid, m_password_entry->Password()});
        ClosePasswordEntry();
        break;
    case PasswordEntry::Result::kCancelled:
        ClosePasswordEntry();
        break;
    case PasswordEntry::Result::kPending:
        if (m_password_entry->IsIdle() || m_state.Get<AS::is_moving>())
        {
            ClosePasswordEntry();
        }
        break;
    }
}

void
WifiPage::ClosePasswordEntry()
{
    m_password_entry = nullptr;
    m_menu.SetExitTimerEnabled(true);
}
