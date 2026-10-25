#include "tile_cache.hh"

#include "png_decoder.hh"

#include <cctype>
#include <mutex>

namespace
{

// The regions around visited places to download, per zoom level
constexpr auto kRegionConfigurations = std::array {
    TileRegions::ZoomConfiguration {.zoom = kDefaultZoom, .region_size = 30, .margin = 7},
    TileRegions::ZoomConfiguration {.zoom = kCityZoom, .region_size = 30, .margin = 7},
    TileRegions::ZoomConfiguration {.zoom = kLandscapeZoom, .region_size = 10, .margin = 2},
};

constexpr auto kRegionSaveInterval = 10s;
// Limit the time spent checking for existing tiles on the SD card in one go
constexpr auto kMaxDiskChecksPerActivation = 32;

constexpr auto kRuntimeOsmApiKeyFilename = "OSM_KEY.TXT";

void
TrimAsciiWhitespace(std::string& value)
{
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

    while (!value.empty() && is_space(static_cast<unsigned char>(value.front())))
    {
        value.erase(value.begin());
    }

    while (!value.empty() && is_space(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
}

class TileHandle final : public ITileHandle
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
                     PostOffice<MSG::AllMessages>& post_office,
                     std::unique_ptr<hal::IPm::ILock> pm_lock,
                     Filesystem& filesystem,
                     HttpsClient& https_client)
    : m_state(application_state)
    , m_post_office(post_office)
    , m_pm_lock(std::move(pm_lock))
    , m_filesystem(filesystem)
    , m_https_client(https_client)
    , m_state_listener(
          m_state.AttachListener<AS::pixel_position, AS::configuration>(GetSemaphore()))
    , m_state_cache(m_state)
    , m_regions(kRegionConfigurations)
    , m_web_thread(std::make_unique<WebThread>(*this))
{
    std::ranges::fill(m_tiles, 0);
}

void
TileCache::OnStartup()
{
    std::string osm_api_key = OSM_API_KEY;
    if (auto runtime_key_data = m_filesystem.ReadFile(kRuntimeOsmApiKeyFilename);
        runtime_key_data && !runtime_key_data->empty())
    {
        std::string runtime_key(reinterpret_cast<const char*>(runtime_key_data->data()),
                                runtime_key_data->size());
        TrimAsciiWhitespace(runtime_key);
        if (!runtime_key.empty())
        {
            osm_api_key = std::move(runtime_key);
        }
    }
    m_web_thread->SetOsmApiKey(std::move(osm_api_key));

    MigrateUnstyledTiles(m_filesystem);

    m_style = m_state.Get<AS::configuration>()->tile_style;
    LoadRegions();

    m_web_thread->Start("web_tile_fetcher", 4096);
}

std::optional<milliseconds>
TileCache::OnActivation()
{
    const auto& co = m_state_cache.Pull();

    co.OnNewValue<AS::configuration>([this](const auto& configuration) {
        if (configuration.tile_style != m_style)
        {
            SetStyle(configuration.tile_style);
        }
    });

    co.OnNewValue<AS::pixel_position>([this](const auto& pixel_position) {
        m_regions.Visit(ToTile(pixel_position));
        m_regions.Visit(ToTile(OsmPointToPoint(pixel_position, kCityZoom)));
        m_regions.Visit(ToTile(OsmPointToPoint(pixel_position, kLandscapeZoom)));
    });

    StyledTile written;
    while (m_written_tiles.pop(written))
    {
        // Not if downloaded before a style change
        if (written.style == m_style)
        {
            m_regions.TileOnDisk(written.tile);
        }
    }

    auto wifi_connected = m_state.Get<AS::wifi_connected>();
    if (wifi_connected && !m_wifi_was_connected)
    {
        // Retry the tiles which failed last time
        m_regions.RestartRound();
    }
    m_wifi_was_connected = wifi_connected;

    FillFromColdStore();
    auto more_to_check = FillFromServer();
    // Again, in case the server has written them to FS
    FillFromColdStore();

    SaveRegionsIfDirty();

    if (more_to_check)
    {
        // Continue checking the disk soon, but let others run in between
        return 10ms;
    }
    if (m_get_from_server.empty())
    {
        return std::nullopt;
    }

    return 75ms;
}

std::optional<std::vector<std::byte>>
TileCache::ReadTile(const Tile& t) const
{
    auto path = GetTilePath(t);

    // Hold the lock if the web thread is writing (see below for motivation)
    auto lock = std::lock_guard(m_filesystem_mutex);

    return m_filesystem.ReadFile(path);
}

void
TileCache::FillFromColdStore()
{
    Tile t = kInvalidTile;

    // Tiles loaded in this pass
    auto loaded_tiles = 0u;

    while (m_get_from_coldstore.pop(t))
    {
        auto tile_id = TileId(t);
        auto cached = std::ranges::find(m_tiles, tile_id);
        if (cached != m_tiles.end())
        {
            // Already cached
            continue;
        }

        //        printf("Getting tile %d,%d from cold store\n", t.x, t.y);
        auto data = ReadTile(t);

        auto wifi_connected = m_state.Get<AS::wifi_connected>();

        if (data)
        {
            auto index = EvictTile();

            auto lock = m_pm_lock->FullPower();
            if (DecodePng(*data, m_image_cache[index]))
            {
                // Successfully decoded, otherwise the evicted tile remains evicted
                m_tiles[index] = tile_id;

                if (loaded_tiles == 0)
                {
                    // Draw the first tile directly, then the rest when all are loaded
                    m_post_office.Send<MSG::tile_loaded>();
                }
                loaded_tiles++;
            }
            else
            {
                m_tiles[index] = 0;
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

    if (loaded_tiles > 1)
    {
        // Awake anyone waiting for tiles (i.e., the UI)
        m_post_office.Send<MSG::tile_loaded>();
    }
}

bool
TileCache::FillFromServer()
{
    if (!m_state.Get<AS::wifi_connected>())
    {
        return false;
    }

    auto disk_checks = 0;
    while (m_web_thread->CanFetchTile())
    {
        Tile t;

        if (!m_get_from_server.empty())
        {
            t = m_get_from_server.back();
            m_get_from_server.pop_back();
        }
        else if (disk_checks >= kMaxDiskChecksPerActivation)
        {
            return true;
        }
        else if (auto next = m_regions.NextMissingTile(); next)
        {
            // Second priority, the regions around visited places
            t = *next;
        }
        else
        {
            break;
        }

        if (t.x < 0 || t.y < 0)
        {
            continue;
        }

        auto path = GetTilePath(t);
        disk_checks++;
        if (m_filesystem.FileExists(path))
        {
            // Already got it, e.g., from being requested by the UI
            m_regions.TileOnDisk(t);
            continue;
        }
        m_web_thread->FetchTile({t, m_style});
    }

    while (!m_reload_tiles_from_server.empty() && m_web_thread->CanFetchTile())
    {
        auto t = m_reload_tiles_from_server.back();

        // Invalid tile
        if (t.x < 0 || t.y < 0)
        {
            m_reload_tiles_from_server.pop_back();
            continue;
        }

        m_web_thread->FetchTile({t, m_style});
        m_reload_tiles_from_server.pop_back();
    }

    return false;
}

std::string
TileCache::GetTilePath(const Tile& t) const
{
    return TilePath(m_style, t);
}

void
TileCache::SaveRegionsIfDirty()
{
    if (!m_regions.IsDirty() || (m_region_save_timer && !m_region_save_timer->IsExpired()))
    {
        return;
    }

    m_filesystem.WriteFile(RegionsPath(m_style), m_regions.Serialize());
    m_regions.ClearDirty();

    // Not too often, since it's written for every downloaded tile. Wakes the thread when expired
    m_region_save_timer = StartTimer(kRegionSaveInterval);
}

void
TileCache::LoadRegions()
{
    m_regions = TileRegions(kRegionConfigurations);

    if (auto data = m_filesystem.ReadFile(RegionsPath(m_style));
        data && !m_regions.Deserialize(*data))
    {
        printf("TileCache: Invalid regions file, starting over\n");
    }
}

void
TileCache::SetStyle(TileStyle style)
{
    // Now, since they are replaced below
    if (m_regions.IsDirty())
    {
        m_filesystem.WriteFile(RegionsPath(m_style), m_regions.Serialize());
    }

    m_style = style;
    LoadRegions();

    // The cached tiles are of the old style, so let the UI request them again
    std::ranges::fill(m_tiles, 0);
    for (auto& image : m_image_cache)
    {
        image.SetUseCount(0);
    }
    m_get_from_server.clear();
    m_reload_tiles_from_server.clear();

    m_post_office.Send<MSG::tile_loaded>();
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
    auto id = TileId(at);
    auto cached = std::ranges::find(m_tiles, id);

    if (cached != m_tiles.end())
    {
        auto& tile = m_image_cache[cached - m_tiles.begin()];

        tile.BumpUseCount();

        return tile;
    }
    else
    {
        if (m_get_from_coldstore.push(at))
        {
            Awake();
        }
    }

    return m_black_tile;
}


// The tile fetcher thread
TileCache::WebThread::WebThread(TileCache& parent)
    : m_parent(parent)
{
}

void
TileCache::WebThread::FetchTile(const StyledTile& t)
{
    if (m_in_queue.push(t))
    {
        Awake();
    }
}

std::optional<milliseconds>
TileCache::WebThread::OnActivation()
{
    StyledTile t;

    while (m_in_queue.pop(t))
    {
        // Not the parent's current style, which might have changed since
        auto url = TileUrl(t.style, t.tile, m_osm_api_key);
        auto path = TilePath(t.style, t.tile);

        printf("TileCache: Need tile %ld/%ld. Getting from WEBBEN\n",
               static_cast<long>(t.tile.x),
               static_cast<long>(t.tile.y));
        auto data = m_parent.m_https_client.Get(url);

        if (data)
        {
            // Better would be to save to a temporary name and then rename, but that doesn't work in esp-idf
            auto lock = std::lock_guard(m_parent.m_filesystem_mutex);

            if (m_parent.m_filesystem.WriteFile(path, {data->data(), data->size()}))
            {
                // If full, it will be found on disk later instead
                m_parent.m_written_tiles.push(t);
            }
        }

        // Fetch more (or retry failed)
        m_parent.Awake();
    }

    return std::nullopt;
}
