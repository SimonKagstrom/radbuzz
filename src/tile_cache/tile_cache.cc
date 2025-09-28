#include "tile_cache.hh"

#include <PNGdec.h>

namespace
{

constexpr auto kPendingCityTilesFileName = "pending.bin";

struct DecodeHelper
{
    DecodeHelper(PNG& png, uint16_t* dst)
        : png(png)
        , dst(dst)
        , offset(0)
    {
    }

    DecodeHelper() = delete;

    PNG& png;
    uint16_t* dst;
    size_t offset;
};

int
PngDraw(PNGDRAW* pDraw)
{
    auto helper = static_cast<DecodeHelper*>(pDraw->pUser);

    helper->png.getLineAsRGB565(
        pDraw, helper->dst + helper->offset, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    helper->offset += pDraw->iWidth;

    return 1;
}


class TileHandle : public ITileHandle
{
public:
    explicit TileHandle(Image& image,
                        uint8_t cache_index,
                        std::atomic<uint32_t>& locked_cache_entries)
        : m_image(image)
        , m_cache_index(cache_index)
        , m_locked_cache_entries(locked_cache_entries)
    {
        m_locked_cache_entries |= (1 << m_cache_index);
    }

    ~TileHandle() final
    {
        // Unlock the cache entry
        m_locked_cache_entries &= ~(1 << m_cache_index);
    }

    const Image& GetImage() const final
    {
        return m_image;
    }

private:
    Image& m_image;
    const uint8_t m_cache_index;
    std::atomic<uint32_t>& m_locked_cache_entries;
};


} // namespace

TileCache::TileCache(ApplicationState& application_state,
                     std::unique_ptr<IGpsPort> gps_port,
                     Filesystem& filesystem,
                     HttpdClient& httpd_client)
    : m_application_state(application_state)
    , m_gps_port(std::move(gps_port))
    , m_filesystem(filesystem)
    , m_httpd_client(httpd_client)
    , m_state_listener(m_application_state.AttachListener(GetSemaphore()))
{
    std::ranges::fill(m_tiles, kInvalidTile);

    m_gps_port->AwakeOn(GetSemaphore());
}

void
TileCache::OnStartup()
{
    auto pending_city_tile_data = m_filesystem.ReadFile(kPendingCityTilesFileName);

    if (pending_city_tile_data && pending_city_tile_data->size() % (2 * sizeof(int32_t)) == 0)
    {
        auto count = pending_city_tile_data->size() / sizeof(int32_t);
        auto ptr = reinterpret_cast<const int32_t*>(pending_city_tile_data->data());

        for (auto i = 0u; i < count; i += 2)
        {
            m_pending_city_tiles.insert(Tile {ptr[i], ptr[i + 1]});
        }
    }

    for (auto city_tile : m_pending_city_tiles)
    {
        RefreshCityTiles(city_tile);
    }
}

std::optional<milliseconds>
TileCache::OnActivation()
{
    if (auto gps_data = m_gps_port->Poll(); gps_data)
    {
        auto city_tile = ToCityTile(gps_data->pixel_position);

        if (m_pending_city_tiles.find(city_tile) == m_pending_city_tiles.end())
        {
            m_pending_city_tiles.insert(city_tile);
            SavePendingCityTiles();
        }

        if (AppState()->wifi_connected && city_tile != m_current_city_tile)
        {
            m_current_city_tile = city_tile;

            auto center_tile = ToTile(gps_data->pixel_position);

            RefreshCityTiles(center_tile);
        }
    }

    FillFromColdStore();
    FillFromServer();
    // Again, in case the server has written them to FS
    FillFromColdStore();

    if (m_get_from_server.empty())
    {
        return std::nullopt;
    }
    else
    {
        return 75ms;
    }
}

bool
TileCache::DecodePng(std::span<const std::byte> png_data, TileImage& out)
{
    auto png = std::make_unique<PNG>();

    auto rc = png->openFLASH((uint8_t*)png_data.data(), png_data.size(), PngDraw);
    if (rc != PNG_SUCCESS)
    {
        return false;
    }


    if (out.Data16().size() == png->getWidth() * png->getHeight())
    {
        DecodeHelper priv(*png, out.WritableData16());
        rc = png->decode((void*)&priv, 0);
    }
    else
    {
        rc = PNG_MEM_ERROR;
    }

    png->close();
    if (rc != PNG_SUCCESS)
    {
        // Failed to decode
        return false;
    }

    return true;
}


void
TileCache::FillFromColdStore()
{
    Tile t = kInvalidTile;

    while (m_get_from_coldstore.pop(t))
    {
        auto cached = std::ranges::find(m_tiles, t);
        if (cached != m_tiles.end())
        {
            // Already cached
            continue;
        }

        printf("Getting tile %d,%d from cold store\n", t.x, t.y);
        auto data = m_filesystem.ReadFile(std::format("tiles/15/{}/{}.png", t.x, t.y));

        auto wifi_connected = AppState()->wifi_connected;

        if (data)
        {
            auto index = EvictTile();

            if (DecodePng(*data, m_image_cache[index]))
            {
                // Successfully decoded, otherwise the evicted tile remains evicted
                m_tiles[index] = t;
            }
            else
            {
                m_tiles[index] = kInvalidTile;
                m_image_cache[index].SetUseCount(0);

                // Reload it from the server
                if (wifi_connected)
                {
                    m_reload_tiles_from_server.push_back(t);
                }
            }
        }
        else if (wifi_connected)
        {
            m_get_from_server.push_back(t);
        }
    }
}

void
TileCache::RefreshCityTiles(const Tile& center)
{
    for (int dx = -kCityTileFactor; dx <= kCityTileFactor; ++dx)
    {
        for (int dy = -kCityTileFactor; dy <= kCityTileFactor; ++dy)
        {
            Tile t {center.x + dx, center.y + dy};

            m_get_from_server_background.push_back(t);
        }
    }
}


void
TileCache::FillFromServer()
{
    if (m_get_from_server.empty() && m_reload_tiles_from_server.empty() &&
        m_get_from_server_background.empty())
    {
        return;
    }

    if (!AppState()->wifi_connected)
    {
        m_get_from_server.clear();
        return;
    }

    while (!m_get_from_server.empty() && !m_get_from_server_background.empty())
    {
        Tile t;

        if (m_get_from_server.empty())
        {
            t = m_get_from_server.back();
            m_get_from_server.pop_back();
        }
        else
        {
            // Second priority, get when the first are empty
            t = m_get_from_server_background.back();
            m_get_from_server_background.pop_back();
        }


        auto path = GetTilePath(t, 15);
        if (m_filesystem.FileExists(path))
        {
            // Already got it, probably from being requested by the UI
            continue;
        }

        printf("TileCache: Need tile %d/%d. Getting from WEBBEN\n", t.x, t.y);
        auto data = m_httpd_client.Get(GetTileUrl(t));

        if (data)
        {
            m_filesystem.WriteFile(path, {data->data(), data->size()});
        }
        break;
    }

    while (!m_reload_tiles_from_server.empty())
    {
        auto t = m_reload_tiles_from_server.back();
        m_reload_tiles_from_server.pop_back();

        auto path = GetTilePath(t, 15);

        printf("TileCache: Need to reload tile %d/%d. Getting from WEBBEN\n", t.x, t.y);
        auto data = m_httpd_client.Get(GetTileUrl(t));

        if (data)
        {
            m_filesystem.WriteFile(path, {data->data(), data->size()});
        }
        break;
    }
}

std::string
TileCache::GetTileUrl(const Tile& t) const
{
    constexpr auto kOsmApiKey = OSM_API_KEY;

    return std::format(
        "https://tile.thunderforest.com/cycle/15/{}/{}.png?apikey={}", t.x, t.y, kOsmApiKey);
}

std::string
TileCache::GetTilePath(const Tile& t, unsigned zoom_level) const
{
    return std::format("tiles/{}/{}/{}.png", zoom_level, t.x, t.y);
}

void
TileCache::SavePendingCityTiles()
{
    std::vector<std::byte> data;
    data.resize(m_pending_city_tiles.size() * sizeof(Tile));

    auto ptr = reinterpret_cast<int32_t*>(data.data());
    for (const auto& t : m_pending_city_tiles)
    {
        *ptr++ = t.x;
        *ptr++ = t.y;
    }

    m_filesystem.WriteFile(kPendingCityTilesFileName, data);
}

uint8_t
TileCache::EvictTile()
{
    uint32_t lowest_use_count = UINT32_MAX;
    uint32_t highest_use_count = 0;
    auto selected = 0u;

    for (auto i = 0u; i < m_tiles.size(); ++i)
    {
        auto& tile = m_image_cache[i];
        auto uc = tile.UseCount();

        /*
         * There is the case where use count wraps, but that should only cause
         * some extra loads from disk.
         */
        if (uc < lowest_use_count)
        {
            lowest_use_count = uc;
            selected = i;
        }
        highest_use_count = std::max(highest_use_count, uc);
    }

    m_image_cache[selected].SetUseCount(highest_use_count + 1);
    return selected;
}

const Image&
TileCache::GetTile(const Tile& at)
{
    auto cached = std::ranges::find(m_tiles, at);

    if (cached != m_tiles.end())
    {
        auto& tile = m_image_cache[cached - m_tiles.begin()];

        tile.BumpUseCount();

        return tile;
    }
    else
    {
        m_get_from_coldstore.push(at);
        Awake();
    }

    return m_black_tile;
}
