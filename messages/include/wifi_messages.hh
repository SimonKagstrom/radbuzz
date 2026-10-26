// Messages to/from the wifi handler
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace MSG
{

enum class WifiType
{
    // Not reported by the wifi client
    kUnknown,
    kOpen,
    kSecured,

    kValueCount,
};

struct WifiScanEntry
{
    std::string ssid;
    // dBm, if known
    std::optional<int8_t> rssi;
    WifiType type {WifiType::kUnknown};

    bool operator==(const WifiScanEntry& other) const = default;
};

// Sent after every finished scan, in the order from the wifi client. Unique, non-empty SSIDs.
struct wifi_scan_result
{
    std::vector<WifiScanEntry> networks;
};

// Scan now, e.g., for the UI to show the networks. Results in a wifi_scan_result
struct start_wifi_scan
{
};

// Store the network first among the known networks (also with a wrong password), and connect
// to it if it's in range. Empty password for open networks
struct wifi_connect
{
    std::string ssid;
    std::string password;
};

using WifiMessages = std::tuple<wifi_scan_result, start_wifi_scan, wifi_connect>;

} // namespace MSG
