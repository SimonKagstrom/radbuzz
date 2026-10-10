#pragma once

#include "king_shark_packet_protocol_base.hh"

#include <array>
#include <etl/queue.h>
#include <etl/vector.h>
#include <optional>
#include <span>

class KingSharkPacketProtocol : public KingSharkPacketProtocolBase<>
{
public:
    std::optional<std::span<const uint8_t>> BuildTxPacket(uint8_t command,
                                                          std::span<const uint8_t> payload);

    void PushData(std::span<const uint8_t> data);

    // Return a payload if a full and valid packet has been received. The payload is valid until
    // the next call to Poll()
    std::optional<std::span<const uint8_t>> Poll();

private:
    // KingSharkPacketProtocolBase, see king_shark_packet_protocol.md
    Header0Next Evaluate(Header0&) final;
    Header1Next Evaluate(Header1&) final;
    CommandNext Evaluate(Command&) final;
    LengthNext Evaluate(Length&) final;
    DataNext Evaluate(Data&) final;
    Checksum0Next Evaluate(Checksum0&) final;
    Checksum1Next Evaluate(Checksum1&) final;
    Footer0Next Evaluate(Footer0&) final;
    Footer1Next Evaluate(Footer1&) final;
    CompleteNext Evaluate(Complete&) final;

    void Exit(Header0&) final;
    void Exit(Header1&) final;
    void Enter(Command&) final;
    void Exit(Command&) final;
    void Exit(Length&) final;
    void Exit(Data&) final;
    void Exit(Checksum0&) final;
    void Exit(Checksum1&) final;
    void Exit(Footer0&) final;
    void Exit(Footer1&) final;
    void Exit(Complete&) final;

    // The next byte, without consuming it
    std::optional<uint8_t> PeekByte() const;
    uint8_t ConsumeByte();

    std::array<uint8_t, 2> CalculateChecksum(std::span<const uint8_t> data) const;

    etl::vector<uint8_t, 128> m_transmit_buffer;
    etl::queue<uint8_t, 128> m_receive_buffer;
    // 0x16, command, length and up to 255 bytes of data
    etl::vector<uint8_t, 3 + 255> m_data_buffer;
    std::array<uint8_t, 2> m_checksum {};
    uint8_t m_length {0};
    bool m_packet_returned {false};
};
