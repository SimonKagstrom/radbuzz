#pragma once

#include "gadget_bridge_protocol.hh"
#include "hal/i_ble_server.hh"
#include "line_collector.hh"

// Nordic UART service, as used by Bangle.js (and thereby Gadgetbridge)
constexpr auto kServiceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
// Written by the phone
constexpr auto kRxCharacteristicUuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
// Notifications to the phone
constexpr auto kTxCharacteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

class GadgetBridgeTransport
{
public:
    GadgetBridgeTransport(hal::IBleServer& server, GadgetBridgeProtocol& protocol);

private:
    void OnData(std::span<const uint8_t> data);

    hal::IBleServer& m_server;
    GadgetBridgeProtocol& m_protocol;
    LineCollector m_line_collector;
};
