#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "hal/i_pm.hh"
#include "https_client.hh"
#include "image.hh"
#include "wgs84_to_osm_point.hh"

#include <array>
#include <atomic>
#include <deque>
#include <etl/queue_spsc_atomic.h>
#include <etl/unordered_set.h>
#include <unordered_map>
#include <unordered_set>

consteval auto
TilesBySize(auto mb)
{
    return mb * 1024 * 1024 / (kTileSize * kTileSize * sizeof(uint16_t));
}
constexpr auto kTileCacheSize = TilesBySize(8);

class ITileHandle
{
public:
    virtual ~ITileHandle() = default;

    virtual const Image& GetImage() const = 0;
};

class TileImage : public SingleColorImage
{
public:
    TileImage()
        : SingleColorImage(kTileSize, kTileSize, 2, 0x0000) // Black tile
    {
    }

    TileImage(const TileImage&) = delete;
    TileImage& operator=(const TileImage&) = delete;

    uint32_t UseCount() const
    {
        return m_use_count.load();
    }

    void BumpUseCount(uint32_t delta = 1)
    {
        m_use_count += delta;
    }

    void SetUseCount(uint32_t count)
    {
        m_use_count = count;
    }

private:
    std::atomic<uint32_t> m_use_count {0};
};

class TileCache : public os::BaseThread
{
public:
    TileCache(ApplicationState& application_state,
              std::unique_ptr<hal::IPm::ILock> pm_lock,
              Filesystem& filesystem,
              HttpsClient& https_client);

    // Context: Another thread (the user interface)
    const Image& GetTile(const Tile& at);

private:
    class WebThread final : public os::BaseThread
    {
    public:
        WebThread(TileCache& parent);

        ~WebThread() final = default;

        bool CanFetchTile() const
        {
            return !m_in_queue.full();
        }

        void FetchTile(const Tile& t);

    private:
        std::string GetTileUrl(const Tile& t) const;

        std::optional<milliseconds> OnActivation() final;

        TileCache& m_parent;
        etl::queue_spsc_atomic<Tile, 8> m_in_queue;
    };

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void FillFromColdStore();
    void FillFromServer();
    void RefreshCityTiles(const Tile& center);

    uint8_t EvictTile();

    bool DecodePng(std::span<const std::byte> png_data, TileImage& out);

    // helpers
    ApplicationState::ReadOnly AppState() const
    {
        return m_application_state.CheckoutReadonly();
    }

    std::string GetTilePath(const Tile& t) const;

    void SavePendingCityTiles();

    ApplicationState& m_application_state;
    std::unique_ptr<hal::IPm::ILock> m_pm_lock;
    Filesystem& m_filesystem;
    HttpsClient& m_https_client;

    std::unique_ptr<ListenerCookie> m_state_listener;
    ApplicationState::PartialReadOnlyCache<AS::pixel_position> m_pixel_state_cache;

    SingleColorImage m_black_tile {kTileSize, kTileSize, 2, 0x0000}; // Black tile

    std::array<TileImage, kTileCacheSize> m_image_cache;
    std::array<uint32_t, kTileCacheSize> m_tiles;

    etl::queue_spsc_atomic<Tile, 8> m_get_from_coldstore;
    std::vector<Tile> m_get_from_server;
    std::vector<Tile> m_get_from_server_background;
    std::vector<Tile> m_reload_tiles_from_server;
    Tile m_current_city_tile {kInvalidTile};

    std::unordered_map<uint8_t, std::unordered_set<Tile>> m_pending_city_tiles_by_zoom;

    std::unique_ptr<WebThread> m_web_thread;
};
