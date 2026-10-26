#pragma once

#include "configuration_settings.hh"
#include "wifi_messages.hh"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// A network in the wifi settings list
struct WifiNetworkListEntry
{
    std::string ssid;
    // dBm, if known
    std::optional<int8_t> rssi;
    MSG::WifiType type {MSG::WifiType::kUnknown};
    bool connected {false};
    // The stored password, for known networks (empty for open ones)
    std::optional<std::string> password;

    bool operator==(const WifiNetworkListEntry& other) const = default;
};

// The networks to show, in order: The connected network (also if it's not in the scan), the
// known networks in range, and then the others. Both groups sorted by signal strength. Known
// networks which are not in range are not included
std::vector<WifiNetworkListEntry>
CreateWifiNetworkList(std::span<const MSG::WifiScanEntry> scan,
                      std::span<const WifiSsidNetwork> known_networks,
                      std::string_view connected_ssid);
