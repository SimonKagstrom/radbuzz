#include "wifi_client_host.hh"

std::vector<std::string>
WifiClientHost::Scan()
{
    return {"Sifod", "LaPlanta", "Zundapp"};
}

void
WifiClientHost::Connect(const char* ssid, const char* password)
{
    // Not a real one here, so OK to print the password
    printf("Connecting to wifi network: %s with password: %s\n", ssid, password);
    m_on_event(hal::IWifiClient::Event::kConnected);
}

void
WifiClientHost::Disconnect()
{
    m_on_event(hal::IWifiClient::Event::kDisconnected);
}

std::unique_ptr<ListenerCookie>
WifiClientHost::AttachListener(std::function<void(Event)> on_event)
{
    m_on_event = on_event;

    return std::make_unique<ListenerCookie>([this]() { m_on_event = [](auto) { /* NOP */ }; });
}
