#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "gps_reader.hh"
#include "httpd_client.hh"
#include "image.hh"
#include "wgs84_to_osm_point.hh"

#include <array>
#include <atomic>
#include <deque>
#include <etl/queue_spsc_atomic.h>

consteval auto
TilesBySize(auto mb)
{
    return mb * 1024 * 1024 / (kTileSize * kTileSize * sizeof(uint16_t));
}
constexpr auto kTileCacheSize = TilesBySize(2);

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

    uint32_t UseCount() const
    {
        return m_use_count.load();
    }

    void BumpUseCount(uint32_t delta = 1)
    {
        m_use_count += delta;
    }

private:
    std::atomic<uint32_t> m_use_count {0};
};

class TileCache : public os::BaseThread
{
public:
    TileCache(ApplicationState& application_state,
              std::unique_ptr<IGpsPort> gps_port,
              Filesystem& filesystem,
              HttpdClient& httpd_client);

    enum class State
    {
        kWaitingForGps,
        kGpsSignalAcquired,
        kDownloadTiles,
        kRunning,

        kValueCount,
    };

    // Context: Another thread
    const Image& GetTile(const Tile& at);

private:
    std::optional<milliseconds> OnActivation() final;

    void RunStateMachine();

    State StateWaitingForGps();
    State StateGpsSignalAcquired();
    State StateDownloadTiles();
    State StateRunning();

    void FillFromColdStore();
    void FillFromServer();
    void RefreshCityTiles(const Tile &center);

    uint8_t EvictTile();

    bool DecodePng(std::span<const std::byte> png_data, TileImage& out);

    // helpers
    const ApplicationState::State* AppState() const
    {
        return m_application_state.CheckoutReadonly();
    }

    ApplicationState& m_application_state;
    std::unique_ptr<IGpsPort> m_gps_port;
    Filesystem& m_filesystem;
    HttpdClient& m_httpd_client;

    State m_state {State::kWaitingForGps};

    std::unique_ptr<ApplicationState::IListener> m_state_listener;

    SingleColorImage m_black_tile {kTileSize, kTileSize, 2, 0x0000}; // Black tile

    std::array<Tile, kTileCacheSize> m_tiles;
    std::array<TileImage, kTileCacheSize> m_image_cache;

    etl::queue_spsc_atomic<Tile, kTileCacheSize> m_get_from_coldstore;
    std::deque<Tile> m_get_from_server;
    Tile m_current_city_tile {kInvalidTile};
};
