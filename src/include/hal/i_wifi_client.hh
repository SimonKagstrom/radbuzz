#pragma once

#include "listener_cookie.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hal
{

class IWifiClient
{
public:
    enum class Event
    {
        kConnected,
        kDisconnected,
        kScanDone,
    };

    struct AccessPoint
    {
        std::string ssid;
        // dBm
        int8_t rssi;
        // Needs a password
        bool secured;
    };

    virtual ~IWifiClient() = default;

    // Turn the wifi on/off
    virtual void Enable() = 0;
    virtual void Disable() = 0;

    // Start a scan in the background, kScanDone is sent when it's finished
    virtual void StartScan() = 0;

    // The access points found by the last finished scan. Only valid once per scan
    virtual std::vector<AccessPoint> GetScanResult() = 0;

    virtual void Connect(const char* ssid, const char* password) = 0;
    virtual void Disconnect() = 0;

    virtual ListenerCookie AttachListener(std::function<void(Event)> on_event) = 0;
};

} // namespace hal
