#include "king_shark_packet_protocol.hh"

#include <algorithm>

constexpr auto kHeaderMagic = std::array {static_cast<uint8_t>(0x3a), static_cast<uint8_t>(0x16)};
constexpr auto kHeaderSize = 4;
constexpr auto kFooterSize = 4;
constexpr auto kFooterMagic = std::array {static_cast<uint8_t>(0x0d), static_cast<uint8_t>(0x0a)};

std::optional<std::span<const uint8_t>>
KingSharkPacketProtocol::BuildTxPacket(uint8_t command, std::span<const uint8_t> payload)
{
    m_transmit_buffer.clear();

    std::ranges::copy(kHeaderMagic, std::back_inserter(m_transmit_buffer));
    m_transmit_buffer.push_back(command);
    m_transmit_buffer.push_back(payload.size());
    std::ranges::copy(payload, std::back_inserter(m_transmit_buffer));
    std::ranges::copy(CalculateChecksum(std::span<const uint8_t>(m_transmit_buffer).subspan(1)),
                       std::back_inserter(m_transmit_buffer));
    std::ranges::copy(kFooterMagic, std::back_inserter(m_transmit_buffer));

    return m_transmit_buffer;
}

// Push packet data, and return a payload if a full and valid packet has been received
std::optional<std::span<const uint8_t>>
KingSharkPacketProtocol::PushData(std::span<const uint8_t> data)
{
    m_receive_buffer.insert(m_receive_buffer.end(), data.begin(), data.end());
    return RunStateMachine();
}

std::optional<std::span<const uint8_t>>
KingSharkPacketProtocol::RunStateMachine()
{
    std::optional<std::span<const uint8_t>> out;
    State before;

    do
    {
        before = m_current_state;

        switch (m_current_state)
        {
        case State::kIdle:
            if (m_receive_buffer.size() != 0)
            {
                m_current_state = State::kWaitForHeader;
            }
            break;

        case State::kWaitForHeader:
            if (m_receive_buffer.size() >= kHeaderSize &&
                std::ranges::starts_with(m_receive_buffer, kHeaderMagic))
            {
                m_length = m_receive_buffer[3];
                m_current_state = State::kWaitForData;
            }
            break;
        case State::kWaitForData:
            if (m_receive_buffer.size() >= kHeaderSize + m_length)
            {
                m_current_state = State::kWaitForFooter;
            }
            break;
        case State::kWaitForFooter:
            if (m_receive_buffer.size() >= kHeaderSize + m_length + kFooterSize)
            {
                // Excluding the checksum
                auto footer_span = std::span<const uint8_t>(
                    m_receive_buffer.data() + kHeaderSize + m_length + 2, 2);

                if (std::ranges::equal(footer_span, kFooterMagic))
                {
                    m_current_state = State::kVerifyData;
                }
            }
            break;
        case State::kVerifyData: {
            auto payload_span =
                std::span<const uint8_t>(m_receive_buffer).subspan(1, kHeaderSize + m_length - 1);
            auto payload_checksum_span =
                std::span<const uint8_t>(m_receive_buffer).subspan(kHeaderSize + m_length, 2);

            if (std::ranges::equal(payload_checksum_span, CalculateChecksum(payload_span)))
            {
                m_current_state = State::kValidData;
            }
            else
            {
                m_receive_buffer.clear();
                m_current_state = State::kIdle;
            }
            break;
        }
        case State::kValidData:
            // Skip the leading bytes, but include command/length
            out = std::span<const uint8_t>(m_receive_buffer).subspan(2, 2 + m_length);
            m_receive_buffer.clear();
            m_current_state = State::kIdle;
            break;
        case State::kValueCount:
            break;
        }
    } while (before != m_current_state);

    return out;
}

std::array<uint8_t, 2>
KingSharkPacketProtocol::CalculateChecksum(std::span<const uint8_t> data) const
{
    auto checksum = std::accumulate(data.begin(), data.end(), 0);

    return {static_cast<uint8_t>(checksum & 0xFF), static_cast<uint8_t>(checksum >> 8)};
}
