#include "king_shark_packet_protocol.hh"

#include <algorithm>
#include <iterator>
#include <numeric>

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
void
KingSharkPacketProtocol::PushData(std::span<const uint8_t> data)
{
    for (auto byte : data)
    {
        if (m_receive_buffer.full())
        {
            // Drop on overflow, the state machine will resync on the next header
            break;
        }
        m_receive_buffer.push(byte);
    }
}

bool
KingSharkPacketProtocol::RunStateMachine(uint8_t byte)
{
    // On errors, restart the search for the header, but don't miss it if it starts here
    const auto resync_state = byte == kHeaderMagic[0] ? State::kHeader1 : State::kHeader0;

    switch (m_current_state)
    {
    case State::kHeader0:
        if (byte == kHeaderMagic[0])
        {
            m_current_state = State::kHeader1;
        }
        break;
    case State::kHeader1:
        if (byte == kHeaderMagic[1])
        {
            // The checksum includes the second header byte
            m_data_buffer.clear();
            m_data_buffer.push_back(byte);
            m_current_state = State::kCommand;
        }
        else
        {
            m_current_state = resync_state;
        }
        break;
    case State::kCommand:
        m_data_buffer.push_back(byte);
        m_current_state = State::kLength;
        break;
    case State::kLength:
        m_length = byte;
        m_data_buffer.push_back(byte);
        m_current_state = m_length == 0 ? State::kChecksum0 : State::kData;
        break;
    case State::kData:
        m_data_buffer.push_back(byte);
        if (m_data_buffer.size() == m_length + 3) // 0x16 + command + length + data
        {
            m_current_state = State::kChecksum0;
        }
        break;
    case State::kChecksum0:
        m_checksum[0] = byte;
        m_current_state = State::kChecksum1;
        break;
    case State::kChecksum1:
        m_checksum[1] = byte;
        m_current_state = State::kFooter0;
        break;
    case State::kFooter0:
        m_current_state = byte == kFooterMagic[0] ? State::kFooter1 : resync_state;
        break;
    case State::kFooter1:
        if (byte == kFooterMagic[1])
        {
            m_current_state = State::kHeader0;

            return std::ranges::equal(m_checksum, CalculateChecksum(m_data_buffer));
        }
        m_current_state = resync_state;
        break;
    case State::kValueCount:
        break;
    }

    return false;
}

std::optional<std::span<const uint8_t>>
KingSharkPacketProtocol::Poll()
{
    while (auto byte = NextByte())
    {
        if (RunStateMachine(*byte))
        {
            // Skip the leading 0x16, but include command/length
            return std::span<const uint8_t>(m_data_buffer).subspan(1);
        }
    }

    return std::nullopt;
}

std::array<uint8_t, 2>
KingSharkPacketProtocol::CalculateChecksum(std::span<const uint8_t> data) const
{
    auto checksum = std::accumulate(data.begin(), data.end(), 0);

    return {static_cast<uint8_t>(checksum & 0xFF), static_cast<uint8_t>(checksum >> 8)};
}
