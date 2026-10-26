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

using WifiMessages = std::tuple<wifi_scan_result>;

} // namespace MSG
