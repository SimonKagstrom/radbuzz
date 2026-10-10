#pragma once

#include "listener_cookie.hh"

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

    virtual ~IWifiClient() = default;

    // Turn the wifi on/off
    virtual void Enable() = 0;
    virtual void Disable() = 0;

    // Start a scan in the background, kScanDone is sent when it's finished
    virtual void StartScan() = 0;

    // The SSIDs found by the last finished scan. Only valid once per scan
    virtual std::vector<std::string> GetScanResult() = 0;

    virtual void Connect(const char* ssid, const char* password) = 0;
    virtual void Disconnect() = 0;

    virtual std::unique_ptr<ListenerCookie> AttachListener(std::function<void(Event)> on_event) = 0;
};

} // namespace hal
