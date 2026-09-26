#pragma once

#include "gadget_bridge_protocol.hh"
#include "hal/i_ble_server.hh"

constexpr auto kServiceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";

class GadgetBridgeTransport
{
public:
    GadgetBridgeTransport(hal::IBleServer& server, GadgetBridgeProtocol& protocol);

private:
    hal::IBleServer& m_server;
    GadgetBridgeProtocol& m_protocol;
};
