#include "wifi_network_list.hh"

#include <algorithm>
#include <limits>

namespace
{

std::optional<std::string>
PasswordOf(std::span<const WifiSsidNetwork> known_networks, std::string_view ssid)
{
    // The first one, like the wifi handler
    auto it = std::ranges::find(known_networks, ssid, &WifiSsidNetwork::ssid);
    if (it == known_networks.end())
    {
        return std::nullopt;
    }

    return it->password;
}

// Strongest first, and networks without a signal strength last. Stable, so equal ones keep
// the scan order
void
SortBySignalStrength(std::vector<WifiNetworkListEntry>& entries)
{
    auto strength = [](const WifiNetworkListEntry& entry) {
        return entry.rssi ? *entry.rssi : std::numeric_limits<int>::min();
    };

    std::ranges::stable_sort(
        entries, [&strength](const auto& a, const auto& b) { return strength(a) > strength(b); });
}

} // namespace

std::vector<WifiNetworkListEntry>
CreateWifiNetworkList(std::span<const MSG::WifiScanEntry> scan,
                      std::span<const WifiSsidNetwork> known_networks,
                      std::string_view connected_ssid)
{
    std::vector<WifiNetworkListEntry> list;
    std::vector<WifiNetworkListEntry> known_in_range;
    std::vector<WifiNetworkListEntry> others;

    auto is_listed = [&](std::string_view ssid) {
        auto in = [ssid](const auto& entries) {
            return std::ranges::find(entries, ssid, &WifiNetworkListEntry::ssid) != entries.end();
        };

        return in(list) || in(known_in_range) || in(others);
    };

    if (!connected_ssid.empty())
    {
        // Might be missing from the scan, e.g., a weak signal
        auto it = std::ranges::find(scan, connected_ssid, &MSG::WifiScanEntry::ssid);
        auto entry = it != scan.end()
                         ? WifiNetworkListEntry {it->ssid, it->rssi, it->type}
                         : WifiNetworkListEntry {std::string(connected_ssid)};

        entry.connected = true;
        entry.password = PasswordOf(known_networks, connected_ssid);
        list.push_back(std::move(entry));
    }

    for (const auto& network : scan)
    {
        // Unique, non-empty SSIDs from the wifi handler, but don't depend on it
        if (network.ssid.empty() || is_listed(network.ssid))
        {
            continue;
        }

        auto entry = WifiNetworkListEntry {network.ssid, network.rssi, network.type};
        entry.password = PasswordOf(known_networks, network.ssid);

        (entry.password ? known_in_range : others).push_back(std::move(entry));
    }

    SortBySignalStrength(known_in_range);
    SortBySignalStrength(others);

    list.insert(list.end(), known_in_range.begin(), known_in_range.end());
    list.insert(list.end(), others.begin(), others.end());

    return list;
}
