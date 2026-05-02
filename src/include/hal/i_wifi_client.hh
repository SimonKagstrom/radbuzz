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
    };

    virtual ~IWifiClient() = default;

    virtual std::vector<std::string> Scan() = 0;

    virtual void Connect(const char* ssid, const char* password) = 0;
    virtual void Disconnect() = 0;

    virtual std::unique_ptr<ListenerCookie> AttachListener(std::function<void(Event)> on_event) = 0;
};

} // namespace hal
