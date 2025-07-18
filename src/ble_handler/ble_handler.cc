// Most of this is based on https://github.com/maisonsmd/esp32-google-maps
#include "ble_handler.hh"

namespace
{

// http://stackoverflow.com/questions/236129/how-to-split-a-string-in-c
void
Split(const std::string& s, char delim, std::vector<std::string>& elems)
{
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
}


std::vector<std::string>
SplitString(const std::string& s, std::string_view delims)
{
    std::vector<std::string> elems;
    for (auto c : delims)
    {
        Split(s, c, elems);
    }

    return elems;
}

uint32_t
StringToKey(std::span<const uint8_t> data)
{
    if (data.size() < 8)
    {
        return kInvalidIconHash;
    }

    auto s = std::string(reinterpret_cast<const char*>(data.data()), 8);

    return std::stoul(s, 0, 16);
}

uint32_t
StringToKey(const std::string& s)
{
    return StringToKey({reinterpret_cast<const uint8_t*>(s.data()), s.size()});
}

} // namespace

BleHandler::BleHandler(hal::IBleServer& server, ApplicationState& state, ImageCache& cache)
    : m_server(server)
    , m_state(state)
    , m_image_cache(cache)
{
    // Add a black image for the invalid icon
    auto invalid_data = std::make_unique<uint8_t[]>(kImageByteSize);
    memset(invalid_data.get(), 0, kImageByteSize);
    m_image_cache.Insert(kInvalidIconHash,
                         kImageWidth,
                         kImageHeight,
                         {static_cast<const uint8_t*>(invalid_data.get()), kImageByteSize});
}

void
BleHandler::OnStartup()
{
    m_ble_poller = StartTimer(20ms, [this]() {
        m_server.PollEvents();

        return 20ms;
    });

    m_server.SetServiceUuid128(hal::detail::StringToUuid128(kServiceUuid));

    m_server.AddWriteGattCharacteristics(hal::detail::StringToUuid128(kChaNav),
                                         [this](auto data) { OnChaNav(data); });

    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaSettings),
        [this](auto data) { printf(": %.*s\n", (int)data.size(), (const char*)data.data()); });
    m_server.AddWriteGattCharacteristics(hal::detail::StringToUuid128(kChaNavTbtIcon),
                                         [this](auto data) { OnIcon(data); });
    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaNavTbtIconDesc), [this](auto data) {
            printf("Ifondesc : %.*s\n", (int)data.size(), (const char*)data.data());
        });
    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaGpsSpeed),
        [this](auto data) { printf(": %.*s\n", (int)data.size(), (const char*)data.data()); });

    m_server.Start();
}

std::optional<milliseconds>
BleHandler::OnActivation()
{
    return std::nullopt;
}

void
BleHandler::OnChaNav(std::span<const uint8_t> data)
{
    auto state = m_state.Checkout();

    /*
     * nextRd=Braxvägen
     * nextRdDesc=
     * distToNext=0 m
     * totalDist=500 m
     * eta=15:51
     * ete=2 min
     * iconHash=a7f7f83332
     */
    for (auto line :
         SplitString(std::string(reinterpret_cast<const char*>(data.data()), data.size()), "\n"))
    {
        auto key_val = SplitString(line, "=");
        auto key = key_val[0];
        std::string val = "";
        if (key_val.size() >= 2)
        {
            val = key_val[1];
        }

        if (key == "iconHash")
        {
            state->current_icon_hash = StringToKey(val);
        }
        if (key == "distToNext")
        {
        }
    }

    printf("ChaNav: %.*s\n", (int)data.size(), (const char*)data.data());
}


void
BleHandler::OnIcon(std::span<const uint8_t> data)
{
    /*
     *           1
     * 01234567890
     * -----------
     * a7f7f83332;
     * ^^^key     ^^^data follows here
     */
    if (data.size() < kImageByteSize + 11)
    {
        printf("Not long enough: %d\n", data.size());
        return;
    }

    auto key = StringToKey(data);

    printf("Inserted 0x%x\n", key);
    m_image_cache.Insert(key, kImageWidth, kImageHeight, data.subspan(10));
}
