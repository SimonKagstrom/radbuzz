#include "king_shark_packet_protocol.hh"
#include "test.hh"

namespace
{

class Fixture
{
public:
    std::vector<uint8_t> PacketData(std::initializer_list<uint8_t> contents)
    {
        std::vector<uint8_t> out;

        for (auto c : contents)
        {
            out.push_back(static_cast<uint8_t>(c));
        }

        return out;
    }

    std::span<const uint8_t> AsSpan(auto contents)
    {
        return std::span<const uint8_t>(contents.data(), contents.size());
    }
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Invalid king shark header magic is dropped")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A, 0x17, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0A}));
    REQUIRE(p.Poll() == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Invalid king shark footer magic is dropped")
{
    KingSharkPacketProtocol p;

    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0B}));
    REQUIRE(p.Poll() == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Invalid king shark checksum is dropped")
{
    KingSharkPacketProtocol p;

    // Checksum should be 0x30
    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x31, 0x00, 0x0D, 0x0A}));
    REQUIRE(p.Poll() == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Short king shark packages await more data")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D}));
    REQUIRE(p.Poll() == std::nullopt);
}


TEST_CASE_FIXTURE(Fixture, "A valid king shark packet is accepted")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0A}));
    auto d = p.Poll();
    REQUIRE(d);
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));
}

TEST_CASE_FIXTURE(Fixture, "A valid king shark packet with huge checksum is accepted")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0xff, 0x2f, 0x01, 0x0D, 0x0A}));
    auto d = p.Poll();

    REQUIRE(d);
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0xff})));
}


TEST_CASE_FIXTURE(Fixture, "Partial king shark packets can be received")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00}));
    auto d = p.Poll();
    REQUIRE(d == std::nullopt);
    p.PushData(PacketData({0x30, 0x00, 0x0D, 0x0A}));
    d = p.Poll();
    REQUIRE(d);
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));
}


TEST_CASE_FIXTURE(Fixture, "Double king shark packets can be received")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0A}));
    auto d = p.Poll();
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));

    p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x01, 0x31, 0x00, 0x0D, 0x0A}));
    d = p.Poll();
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x01})));
}

TEST_CASE_FIXTURE(Fixture, "Consecutive king shark packets can be received")
{
    KingSharkPacketProtocol p;
    p.PushData(PacketData({0x3A,
                           0x16,
                           0x19,
                           0x01,
                           0x00,
                           0x30,
                           0x00,
                           0x0D,
                           0x0A,
                           0x3A,
                           0x16,
                           0x19,
                           0x01,
                           0x01,
                           0x31,
                           0x00,
                           0x0D,
                           0x0A}));
    auto d = p.Poll();
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));

    /*
    Not yet handled...
    d = p.PushData({});
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x01})));
    */
}


TEST_CASE_FIXTURE(Fixture, "King shark packets can be serialized")
{
    KingSharkPacketProtocol p;
    auto d = p.BuildTxPacket(0x19, PacketData({0x03}));
    REQUIRE(d);
    REQUIRE(
        std::ranges::equal(*d, PacketData({0x3A, 0x16, 0x19, 0x01, 0x03, 0x33, 0x00, 0x0D, 0x0A})));
}
