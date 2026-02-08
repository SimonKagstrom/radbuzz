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
    REQUIRE(p.PushData(PacketData({0x3A, 0x17, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0A})) ==
            std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Invalid king shark footer magic is dropped")
{
    KingSharkPacketProtocol p;

    REQUIRE(p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0B})) ==
            std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Invalid king shark checksum is dropped")
{
    KingSharkPacketProtocol p;

    // Checksum should be 0x30
    REQUIRE(p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x31, 0x00, 0x0D, 0x0A})) ==
            std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Short king shark packages await more data")
{
    KingSharkPacketProtocol p;
    REQUIRE(p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D})) ==
            std::nullopt);
}


TEST_CASE_FIXTURE(Fixture, "A valid king shark packet is accepted")
{
    KingSharkPacketProtocol p;
    auto d = p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0A}));
    REQUIRE(d);
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));
}


TEST_CASE_FIXTURE(Fixture, "Partial king shark packets can be received")
{
    KingSharkPacketProtocol p;
    auto d = p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00}));
    REQUIRE(d == std::nullopt);
    d = p.PushData(PacketData({0x30, 0x00, 0x0D, 0x0A}));
    REQUIRE(d);
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));
}


TEST_CASE_FIXTURE(Fixture, "Double king shark packets can be received")
{
    KingSharkPacketProtocol p;
    auto d = p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x00, 0x30, 0x00, 0x0D, 0x0A}));
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));

    d = p.PushData(PacketData({0x3A, 0x16, 0x19, 0x01, 0x01, 0x31, 0x00, 0x0D, 0x0A}));
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x01})));
}

TEST_CASE_FIXTURE(Fixture, "Consecutive king shark packets can be received")
{
    KingSharkPacketProtocol p;
    auto d = p.PushData(PacketData({0x3A,
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
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x00})));

    /*
    Not yet handled...
    d = p.PushData({});
    REQUIRE(std::ranges::equal(*d, PacketData({0x19, 0x01, 0x01})));
    */
}
