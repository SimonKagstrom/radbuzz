#include "tile_cache.hh"

namespace
{

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
                     Filesystem& filesystem,
                     HttpdClient& httpd_client)
    : m_application_state(application_state)
    , m_filesystem(filesystem)
    , m_httpd_client(httpd_client)
    , m_state_listener(m_application_state.AttachListener(GetSemaphore()))
{
    std::ranges::fill(m_tiles, kInvalidTile);
}

std::optional<milliseconds>
TileCache::OnActivation()
{
    FillFromColdStore();
    FillFromServer();

    RunStateMachine();
    return std::nullopt;
}

void
TileCache::FillFromColdStore()
{
    Tile t = kInvalidTile;

    while (m_get_from_coldstore.pop(t))
    {
        auto data = m_filesystem.ReadFile(std::format("tiles/15/{}/{}.tile", t.x, t.y));
        if (data)
        {
            auto index = EvictTile();
            m_tiles[index] = t;
            // Decode PNG...
        }
        else
        {
            m_get_from_server.push_back(t);
        }
    }
}

void
TileCache::FillFromServer()
{
    constexpr auto kOsmApiKey = OSM_API_KEY;

    if (!AppState()->wifi_connected)
    {
        m_get_from_server.clear();
        return;
    }

    for (auto& t : m_get_from_server)
    {
        auto data = m_httpd_client.Get(std::format(
            "https://tile.thunderforest.com/cycle/15/{}/{}.png?apikey={}", t.x, t.y, kOsmApiKey));

        if (data)
        {
            m_filesystem.WriteFile(std::format("tiles/15/{}/{}.tile", t.x, t.y),
                                   {data->data(), data->size()});
        }
    }

    m_get_from_server.clear();
}


uint8_t
TileCache::EvictTile()
{
    for (auto i = 0u; i < m_tiles.size(); ++i)
    {
        if (m_tiles[i] == kInvalidTile)
        {
            return i;
        }
    }

    return 0; // TODO: LRU
}

const Image&
TileCache::GetTile(const Tile& at)
{
    auto cached = std::ranges::find(m_tiles, at);

    if (cached != m_tiles.end())
    {
        return m_image_cache[cached - m_tiles.begin()];
    }
    else
    {
        m_get_from_coldstore.push(at);
        Awake();
    }

    return m_black_tile;
}

void
TileCache::RunStateMachine()
{
    auto before = m_state;
    do
    {
        before = m_state;
        switch (m_state)
        {
        case State::kWaitingForGps:
            m_state = StateWaitingForGps();
            break;
        case State::kGpsSignalAcquired:
            m_state = StateGpsSignalAcquired();
            break;
        case State::kDownloadTiles:
            m_state = StateDownloadTiles();
            break;
        case State::kRunning:
            m_state = StateRunning();
            break;

        case State::kValueCount:
            break;
        }


    } while (m_state != before);
}

// The state machine implementation
TileCache::State
TileCache::StateWaitingForGps()
{
    if (AppState()->gps_position_valid)
    {
        return State::kGpsSignalAcquired;
    }

    return State::kWaitingForGps;
}

TileCache::State
TileCache::StateGpsSignalAcquired()
{
    if (AppState()->wifi_connected)
    {
        return State::kDownloadTiles;
    }

    return State::kGpsSignalAcquired;
}

TileCache::State
TileCache::StateDownloadTiles()
{
    return State::kDownloadTiles;
}

TileCache::State
TileCache::StateRunning()
{
    return State::kRunning;
}
