#pragma once

#include <array>
#include <etl/queue.h>
#include <etl/vector.h>
#include <optional>
#include <span>

class KingSharkPacketProtocol
{
public:
    std::optional<std::span<const uint8_t>> BuildTxPacket(uint8_t command,
                                                          std::span<const uint8_t> payload);

    // Push packet data, and return a payload if a full and valid packet has been received
    void PushData(std::span<const uint8_t> data);
    std::optional<std::span<const uint8_t>> Poll();

private:
    enum class State
    {
        kHeader0,
        kHeader1,
        kCommand,
        kLength,

        kData,
        kChecksum0,
        kChecksum1,
        kFooter0,
        kFooter1,

        kValueCount,
    };

    std::optional<uint8_t> NextByte()
    {
        if (m_receive_buffer.empty())
        {
            return std::nullopt;
        }

        auto byte = m_receive_buffer.front();
        m_receive_buffer.pop();
        return byte;
    }


    // Run the state machine for one byte, return true when a full and valid packet is received
    bool RunStateMachine(uint8_t byte);

    std::array<uint8_t, 2> CalculateChecksum(std::span<const uint8_t> data) const;

    etl::vector<uint8_t, 128> m_transmit_buffer;
    etl::queue<uint8_t, 128> m_receive_buffer;
    // 0x16, command, length and up to 255 bytes of data
    etl::vector<uint8_t, 3 + 255> m_data_buffer;
    std::array<uint8_t, 2> m_checksum {};

    State m_current_state {State::kHeader0};
    // State data
    uint8_t m_length {0};
};
