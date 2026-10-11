#include "test.hh"
#include "tile_regions.hh"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{

// Small regions to keep the tests readable. Zoom 3 is 8x8 tiles, so the regions at the right and
// bottom edges are partly outside the world
constexpr auto kConfigurations = std::array {
    TileRegions::ZoomConfiguration {.zoom = 15, .region_size = 4, .margin = 1},
    TileRegions::ZoomConfiguration {.zoom = 3, .region_size = 3, .margin = 0},
};

std::vector<Tile>
AllMissingTiles(TileRegions& regions)
{
    std::vector<Tile> out;

    while (auto tile = regions.NextMissingTile())
    {
        out.push_back(*tile);
    }

    return out;
}

bool
Contains(const std::vector<Tile>& tiles, const Tile& tile)
{
    return std::ranges::find(tiles, tile) != tiles.end();
}

void
PutAllOnDisk(TileRegions& regions)
{
    for (const auto& tile : AllMissingTiles(regions))
    {
        regions.TileOnDisk(tile);
    }
}

} // namespace


TEST_SUITE_BEGIN("tile_regions");

TEST_CASE("nothing is missing without visits")
{
    TileRegions regions(kConfigurations);

    REQUIRE(regions.NextMissingTile() == std::nullopt);
    REQUIRE(regions.PendingRegionCount() == 0);
    REQUIRE_FALSE(regions.IsDirty());
}

TEST_CASE("visiting a tile marks its region as pending")
{
    TileRegions regions(kConfigurations);

    // In the middle of the region 100..103, 200..203
    regions.Visit({101, 201, 15});

    REQUIRE(regions.PendingRegionCount() == 1);
    REQUIRE(regions.IsDirty());

    THEN("all tiles of the region are returned once")
    {
        auto tiles = AllMissingTiles(regions);

        REQUIRE(tiles.size() == 16);
        for (auto y = 200; y < 204; y++)
        {
            for (auto x = 100; x < 104; x++)
            {
                REQUIRE(Contains(tiles, {x, y, 15}));
            }
        }

        AND_THEN("they are not returned again in the same round")
        {
            REQUIRE(regions.NextMissingTile() == std::nullopt);
        }
    }

    WHEN("all tiles are on disk")
    {
        PutAllOnDisk(regions);

        THEN("the region is completed")
        {
            REQUIRE(regions.PendingRegionCount() == 0);
            REQUIRE(regions.CompletedRegionCount() == 1);
        }

        AND_WHEN("it's visited again")
        {
            regions.ClearDirty();
            regions.Visit({102, 202, 15});
            regions.RestartRound();

            THEN("nothing has to be checked")
            {
                REQUIRE(regions.PendingRegionCount() == 0);
                REQUIRE(regions.NextMissingTile() == std::nullopt);
                REQUIRE_FALSE(regions.IsDirty());
            }
        }
    }
}

TEST_CASE("tiles near the region edge also mark the neighbouring regions")
{
    TileRegions regions(kConfigurations);

    SUBCASE("near a side")
    {
        regions.Visit({103, 201, 15});
        REQUIRE(regions.PendingRegionCount() == 2);
        REQUIRE(Contains(AllMissingTiles(regions), {104, 201, 15}));
    }

    SUBCASE("near a corner")
    {
        regions.Visit({100, 200, 15});
        REQUIRE(regions.PendingRegionCount() == 4);
        REQUIRE(Contains(AllMissingTiles(regions), {99, 199, 15}));
    }

    SUBCASE("at the top left of the world")
    {
        regions.Visit({0, 0, 15});
        REQUIRE(regions.PendingRegionCount() == 1);

        auto tiles = AllMissingTiles(regions);
        REQUIRE(tiles.size() == 16);
        REQUIRE(std::ranges::all_of(tiles, [](auto& t) { return t.x >= 0 && t.y >= 0; }));
    }
}

TEST_CASE("tiles outside the world are never returned")
{
    TileRegions regions(kConfigurations);

    // The bottom right region at zoom 3 is 6..8, but the world is only 0..7
    regions.Visit({7, 7, 3});

    auto tiles = AllMissingTiles(regions);
    REQUIRE(tiles.size() == 4);
    REQUIRE(std::ranges::all_of(tiles, [](auto& t) { return t.x < 8 && t.y < 8; }));

    WHEN("the tiles inside are on disk")
    {
        for (const auto& tile : tiles)
        {
            regions.TileOnDisk(tile);
        }

        THEN("the region is completed")
        {
            REQUIRE(regions.CompletedRegionCount() == 1);
        }
    }
}

TEST_CASE("invalid visits are ignored")
{
    TileRegions regions(kConfigurations);

    regions.Visit({100, 200, 14}); // Unconfigured zoom
    regions.Visit({-1, 2, 15});
    regions.Visit({8, 2, 3}); // Outside the world at zoom 3
    regions.TileOnDisk({-5, -5, 15});
    regions.TileOnDisk({100, 200, 14});

    REQUIRE(regions.PendingRegionCount() == 0);
    REQUIRE(regions.NextMissingTile() == std::nullopt);
    REQUIRE_FALSE(regions.IsDirty());
}

TEST_CASE("tiles already on disk are skipped, and missing ones are retried in the next round")
{
    TileRegions regions(kConfigurations);
    regions.Visit({101, 201, 15});

    // E.g., downloaded when shown in the UI
    regions.TileOnDisk({100, 200, 15});
    regions.TileOnDisk({101, 200, 15});

    auto tiles = AllMissingTiles(regions);
    REQUIRE(tiles.size() == 14);
    REQUIRE_FALSE(Contains(tiles, {100, 200, 15}));

    // Two downloads fail, the others succeed
    for (auto i = 2u; i < tiles.size(); i++)
    {
        regions.TileOnDisk(tiles[i]);
    }
    REQUIRE(regions.NextMissingTile() == std::nullopt);
    REQUIRE(regions.PendingRegionCount() == 1);

    regions.RestartRound();
    auto retried = AllMissingTiles(regions);
    REQUIRE(retried.size() == 2);
    REQUIRE(Contains(retried, tiles[0]));
    REQUIRE(Contains(retried, tiles[1]));
}

TEST_CASE("the most recently visited region is handled first")
{
    TileRegions regions(kConfigurations);

    regions.Visit({101, 201, 15});
    regions.Visit({501, 601, 15});
    REQUIRE(regions.NextMissingTile()->x >= 500);

    regions.RestartRound();
    regions.Visit({102, 202, 15});
    REQUIRE(regions.NextMissingTile()->x < 500);
}

TEST_CASE("visiting the same tile again after restoring marks the region again")
{
    TileRegions regions(kConfigurations);
    auto empty = TileRegions(kConfigurations).Serialize();

    regions.Visit({101, 201, 15});
    REQUIRE(regions.PendingRegionCount() == 1);

    // E.g., an old file without the region
    REQUIRE(regions.Deserialize(empty));
    REQUIRE(regions.PendingRegionCount() == 0);

    regions.Visit({101, 201, 15});
    REQUIRE(regions.PendingRegionCount() == 1);
}

TEST_CASE("regions survive serialization")
{
    TileRegions regions(kConfigurations);

    regions.Visit({101, 201, 15});
    regions.Visit({501, 601, 15});
    regions.Visit({1, 1, 3});
    PutAllOnDisk(regions); // Everything completed...
    regions.RestartRound();

    regions.Visit({901, 901, 15}); // ... except this
    regions.TileOnDisk({900, 900, 15});
    regions.TileOnDisk({903, 903, 15});

    auto data = regions.Serialize();

    TileRegions restored(kConfigurations);
    REQUIRE(restored.Deserialize(data));

    REQUIRE(restored.PendingRegionCount() == 1);
    REQUIRE(restored.CompletedRegionCount() == 3);
    REQUIRE_FALSE(restored.IsDirty());

    auto tiles = AllMissingTiles(restored);
    REQUIRE(tiles.size() == 14);
    REQUIRE_FALSE(Contains(tiles, {900, 900, 15}));
    REQUIRE_FALSE(Contains(tiles, {903, 903, 15}));

    // Completed regions stay completed
    restored.Visit({101, 201, 15});
    REQUIRE(restored.PendingRegionCount() == 1);
}

TEST_CASE("regions with a changed configuration are dropped when restored")
{
    TileRegions regions(kConfigurations);
    regions.Visit({101, 201, 15});
    regions.Visit({1, 1, 3});

    // Zoom 15 with another region size, and zoom 3 removed
    constexpr auto kOtherConfigurations = std::array {
        TileRegions::ZoomConfiguration {.zoom = 15, .region_size = 8, .margin = 1},
    };
    TileRegions restored(kOtherConfigurations);

    REQUIRE(restored.Deserialize(regions.Serialize()));
    REQUIRE(restored.PendingRegionCount() == 0);
}

TEST_CASE("broken serialized data is rejected")
{
    TileRegions regions(kConfigurations);
    regions.Visit({101, 201, 15});
    regions.Visit({1, 1, 3});
    PutAllOnDisk(regions);
    regions.Visit({501, 601, 15});

    auto good = regions.Serialize();

    TileRegions restored(kConfigurations);
    restored.Visit({1001, 1001, 15});

    auto require_rejected = [&restored](std::span<const std::byte> data) {
        REQUIRE_FALSE(restored.Deserialize(data));

        // The old state is kept
        REQUIRE(restored.PendingRegionCount() == 1);
        REQUIRE(restored.CompletedRegionCount() == 0);
    };

    SUBCASE("empty")
    {
        require_rejected({});
    }

    SUBCASE("wrong magic")
    {
        auto data = good;
        data[0] = std::byte {0xff};
        require_rejected(data);
    }

    SUBCASE("truncated anywhere")
    {
        for (auto size = 0u; size < good.size(); size++)
        {
            require_rejected(std::span(good).first(size));
        }
    }

    SUBCASE("trailing garbage")
    {
        auto data = good;
        data.push_back(std::byte {0});
        require_rejected(data);
    }

    SUBCASE("huge counts")
    {
        auto data = good;
        uint32_t count = 0xffffffff;
        memcpy(data.data() + 4, &count, sizeof(count));
        require_rejected(data);
    }

    SUBCASE("region outside the world")
    {
        TileRegions source(kConfigurations);
        source.Visit({7, 7, 3});
        auto data = source.Serialize();

        // The region x of the only pending region, after the header and the zoom
        int32_t x = 100;
        memcpy(data.data() + 12 + 4, &x, sizeof(x));
        require_rejected(data);
    }
}

TEST_SUITE_END();
