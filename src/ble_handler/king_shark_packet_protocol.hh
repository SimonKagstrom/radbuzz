#pragma once

#include <array>
#include <etl/vector.h>
#include <optional>
#include <span>

class KingSharkPacketProtocol
{
public:
    std::optional<std::span<const uint8_t>> BuildTxPacket(uint8_t command,
                                                          std::span<const uint8_t> payload);

    // Push packet data, and return a payload if a full and valid packet has been received
    std::optional<std::span<const uint8_t>> PushData(std::span<const uint8_t> data);

private:
    enum class State
    {
        kIdle,
        kWaitForHeader,
        kWaitForData,
        kWaitForFooter,
        kVerifyData,
        kValidData,

        kValueCount,
    };

    std::optional<std::span<const uint8_t>> RunStateMachine();

    std::array<uint8_t, 2> CalculateChecksum(std::span<const uint8_t> data) const;

    etl::vector<uint8_t, 128> m_transmit_buffer;
    etl::vector<uint8_t, 128> m_receive_buffer;

    State m_current_state {State::kIdle};
    // State data
    uint8_t m_length {0};
};
