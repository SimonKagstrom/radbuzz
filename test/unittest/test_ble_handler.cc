#include "ble_handler.hh"
#include "test.hh"
#include "thread_fixture.hh"

#include <map>

namespace
{

class BleServerStub : public hal::IBleServer
{
public:
    void Inject(auto uuid, const std::string& data)
    {
        Inject(uuid, {reinterpret_cast<const uint8_t*>(data.data()), data.size()});
    }

    void Inject(auto uuid, std::span<const uint8_t> data)
    {
        auto uuid128 = hal::detail::StringToUuid128(uuid);
        REQUIRE(m_uuid_cb.find(uuid128[0]) != m_uuid_cb.end());

        m_uuid_cb[uuid128[0]](data);
    }

private:
    void SetServiceUuid128(std::span<const uint8_t, 16> service_uuid) final
    {
    }

    void AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final
    {
        // For now assume they are unique
        REQUIRE(m_uuid_cb.find(uuid[0]) == m_uuid_cb.end());
        m_uuid_cb[uuid[0]] = data;
    }

    void Start() final
    {
    }

    void PollEvents() final
    {
    }

    std::map<uint8_t, std::function<void(std::span<const uint8_t>)>> m_uuid_cb;
};


class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(&ble);
    }

    BleServerStub srv;
    ApplicationState state;
    ImageCache cache;

    BleHandler ble {srv, state, cache};
};

} // namespace


TEST_CASE_FIXTURE(Fixture, "the BLE handler can handle icons")
{
    constexpr auto kPaletteSize = 8;

    uint32_t key = 0xa7f7f833;
    std::string icon_header = "a7f7f83332;";
    auto icon_data = std::array<uint8_t, kImageByteSize> {};

    std::ranges::fill(icon_data, 0x7f);
    DoStartup();

    WHEN("an icon with the proper data comes in")
    {
        std::vector<uint8_t> full_data;

        std::ranges::copy(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(icon_header.data()),
                                     icon_header.size()),
            std::back_inserter(full_data));

        std::ranges::copy(icon_data, std::back_inserter(full_data));
        // The hash + the semi-colon
        REQUIRE(full_data.size() == 10 + 1 + kImageByteSize);


        srv.Inject(kChaNavTbtIcon, full_data);
        DoRunLoop();

        auto in_cache = cache.Lookup(key);
        THEN("it's placed in the cache")
        {
            REQUIRE(in_cache != nullptr);
        }
        AND_THEN("the image is of the correct size")
        {
            const auto& image_dsc = in_cache->GetDsc();
            REQUIRE(image_dsc.data_size == kImageByteSize + kPaletteSize);
            REQUIRE(image_dsc.header.w == kImageWidth);
            REQUIRE(image_dsc.header.h == kImageHeight);
        }
    }

    WHEN("a too short icon comes in")
    {
        srv.Inject(kChaNavTbtIcon, icon_header);
        DoRunLoop();

        THEN("it's not placed in the cache")
        {
            REQUIRE(cache.Lookup(key) == nullptr);
        }
    }
}


TEST_CASE_FIXTURE(Fixture, "the BLE handler can handle navigation info")
{
    DoStartup();

    WHEN("a non-empty description comes in")
    {
        auto non_empty = R"VOBB(nextRd=Braxvägen
nextRdDesc=
distToNext=
totalDist=30 m
eta=15:19
ete=0 min
iconHash=a7f7f83332
        )VOBB";

        srv.Inject(kChaNav, non_empty);

        THEN("the key is updated")
        {
            REQUIRE(state.CheckoutReadonly()->current_icon_hash == 0xa7f7f833);
        }
    }
}
