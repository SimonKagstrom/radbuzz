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

std::optional<std::span<const uint8_t>>
KingSharkPacketProtocol::Poll()
{
    // A packet returned by the last call can now be dropped
    m_packet_returned = CurrentState() == State::kComplete;

    RunStateMachine();

    if (CurrentState() == State::kComplete)
    {
        // Skip the leading 0x16, but include command/length
        return std::span<const uint8_t>(m_data_buffer).subspan(1);
    }

    return std::nullopt;
}

std::optional<uint8_t>
KingSharkPacketProtocol::PeekByte() const
{
    if (m_receive_buffer.empty())
    {
        return std::nullopt;
    }

    return m_receive_buffer.front();
}

uint8_t
KingSharkPacketProtocol::ConsumeByte()
{
    // Only called when leaving a state, i.e., after a successful PeekByte()
    debug_assert(!m_receive_buffer.empty());

    auto byte = m_receive_buffer.front();
    m_receive_buffer.pop();

    return byte;
}

KingSharkPacketProtocol::Header0::Next
KingSharkPacketProtocol::Evaluate(Header0&)
{
    auto byte = PeekByte();
    if (!byte)
    {
        return kStay;
    }
    if (*byte == kHeaderMagic[0])
    {
        return State::kHeader1;
    }

    return State::kHeader0;
}

void
KingSharkPacketProtocol::Exit(Header0&)
{
    ConsumeByte();
}

KingSharkPacketProtocol::Header1::Next
KingSharkPacketProtocol::Evaluate(Header1&)
{
    auto byte = PeekByte();
    if (!byte)
    {
        return kStay;
    }
    if (*byte == kHeaderMagic[1])
    {
        return State::kCommand;
    }
    if (*byte == kHeaderMagic[0])
    {
        return State::kHeader1;
    }

    return State::kHeader0;
}

void
KingSharkPacketProtocol::Exit(Header1&)
{
    ConsumeByte();
}

void
KingSharkPacketProtocol::Enter(Command&)
{
    // The checksum includes the second header byte
    m_data_buffer.clear();
    m_data_buffer.push_back(kHeaderMagic[1]);
}

KingSharkPacketProtocol::Command::Next
KingSharkPacketProtocol::Evaluate(Command&)
{
    if (!PeekByte())
    {
        return kStay;
    }

    return State::kLength;
}

void
KingSharkPacketProtocol::Exit(Command&)
{
    m_data_buffer.push_back(ConsumeByte());
}

KingSharkPacketProtocol::Length::Next
KingSharkPacketProtocol::Evaluate(Length&)
{
    auto byte = PeekByte();
    if (!byte)
    {
        return kStay;
    }
    if (*byte == 0)
    {
        return State::kChecksum0;
    }

    return State::kData;
}

void
KingSharkPacketProtocol::Exit(Length&)
{
    m_length = ConsumeByte();
    m_data_buffer.push_back(m_length);
}

KingSharkPacketProtocol::Data::Next
KingSharkPacketProtocol::Evaluate(Data&)
{
    if (!PeekByte())
    {
        return kStay;
    }
    // 0x16 + command + length + data, including the byte about to be consumed
    if (m_data_buffer.size() + 1 == m_length + 3u)
    {
        return State::kChecksum0;
    }

    return State::kData;
}

void
KingSharkPacketProtocol::Exit(Data&)
{
    m_data_buffer.push_back(ConsumeByte());
}

KingSharkPacketProtocol::Checksum0::Next
KingSharkPacketProtocol::Evaluate(Checksum0&)
{
    if (!PeekByte())
    {
        return kStay;
    }

    return State::kChecksum1;
}

void
KingSharkPacketProtocol::Exit(Checksum0&)
{
    m_checksum[0] = ConsumeByte();
}

KingSharkPacketProtocol::Checksum1::Next
KingSharkPacketProtocol::Evaluate(Checksum1&)
{
    if (!PeekByte())
    {
        return kStay;
    }

    return State::kFooter0;
}

void
KingSharkPacketProtocol::Exit(Checksum1&)
{
    m_checksum[1] = ConsumeByte();
}

KingSharkPacketProtocol::Footer0::Next
KingSharkPacketProtocol::Evaluate(Footer0&)
{
    auto byte = PeekByte();
    if (!byte)
    {
        return kStay;
    }
    if (*byte == kFooterMagic[0])
    {
        return State::kFooter1;
    }
    if (*byte == kHeaderMagic[0])
    {
        return State::kHeader1;
    }

    return State::kHeader0;
}

void
KingSharkPacketProtocol::Exit(Footer0&)
{
    ConsumeByte();
}

KingSharkPacketProtocol::Footer1::Next
KingSharkPacketProtocol::Evaluate(Footer1&)
{
    auto byte = PeekByte();
    if (!byte)
    {
        return kStay;
    }
    if (*byte == kFooterMagic[1] &&
        std::ranges::equal(m_checksum, CalculateChecksum(m_data_buffer)))
    {
        return State::kComplete;
    }
    if (*byte == kHeaderMagic[0])
    {
        return State::kHeader1;
    }

    return State::kHeader0;
}

void
KingSharkPacketProtocol::Exit(Footer1&)
{
    ConsumeByte();
}

KingSharkPacketProtocol::Complete::Next
KingSharkPacketProtocol::Evaluate(Complete&)
{
    if (m_packet_returned)
    {
        return State::kHeader0;
    }

    return kStay;
}

void
KingSharkPacketProtocol::Exit(Complete&)
{
    m_packet_returned = false;
}

std::array<uint8_t, 2>
KingSharkPacketProtocol::CalculateChecksum(std::span<const uint8_t> data) const
{
    auto checksum = std::accumulate(data.begin(), data.end(), 0);

    return {static_cast<uint8_t>(checksum & 0xFF), static_cast<uint8_t>(checksum >> 8)};
}
