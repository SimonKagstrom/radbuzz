#include "gadget_bridge_transport.hh"

GadgetBridgeTransport::GadgetBridgeTransport(hal::IBleServer& server,
                                             GadgetBridgeProtocol& protocol)
    : m_server(server)
    , m_protocol(protocol)
{
    m_server.SetServiceUuid128(hal::detail::StringToUuid128(kServiceUuid));
    m_server.AddWriteGattCharacteristics(hal::detail::StringToUuid128(kRxCharacteristicUuid),
                                         [this](auto data) { OnData(data); });
    // Gadgetbridge requires this to exist, even though nothing is sent yet
    m_server.AddNotifyGattCharacteristics(hal::detail::StringToUuid128(kTxCharacteristicUuid));
}

void
GadgetBridgeTransport::OnData(std::span<const uint8_t> data)
{
    m_line_collector.Push({reinterpret_cast<const char*>(data.data()), data.size()});

    while (auto line = m_line_collector.Poll())
    {
        m_protocol.PushLine(*line);
    }
}
