#pragma once

#include "../i_wifi_client.hh"
#include "test.hh"

class MockWifiClient : public hal::IWifiClient
{
public:
    MAKE_MOCK0(Enable, void(), override);
    MAKE_MOCK0(Disable, void(), override);
    MAKE_MOCK0(StartScan, void(), override);
    MAKE_MOCK0(GetScanResult, std::vector<std::string>(), override);
    MAKE_MOCK2(Connect, void(const char*, const char*), override);
    MAKE_MOCK0(Disconnect, void(), override);
    MAKE_MOCK1(AttachListener,
               std::unique_ptr<ListenerCookie>(std::function<void(Event)>),
               override);
};
