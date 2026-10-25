#include "mock_filesystem.hh"
#include "test.hh"
#include "tile_styles.hh"

#include <set>

namespace
{

std::string
Contents(const Filesystem& filesystem, std::string_view path)
{
    auto data = filesystem.ReadFile(path);
    REQUIRE(data);

    return std::string(reinterpret_cast<const char*>(data->data()), data->size());
}

} // namespace


TEST_SUITE_BEGIN("tile_styles");

TEST_CASE("the paths include the style directory")
{
    REQUIRE(TilePath(TileStyle::kCycle, {17600, 9300, 15}) == "tiles/cycle/15/17600/9300.png");
    REQUIRE(RegionsPath(TileStyle::kCycle) == "pending/cycle/regions.bin");
}

TEST_CASE("each style has its own name, URL and directory")
{
    std::set<std::string_view> names;
    std::set<std::string> urls;
    std::set<std::string> paths;

    for (auto i = 0u; i < std::to_underlying(TileStyle::kValueCount); i++)
    {
        auto style = static_cast<TileStyle>(i);

        names.insert(TileStyleName(style));
        urls.insert(TileUrl(style, {1, 2, 10}, "key"));
        paths.insert(TilePath(style, {1, 2, 10}));
    }

    REQUIRE(names.size() == std::to_underlying(TileStyle::kValueCount));
    REQUIRE(urls.size() == names.size());
    REQUIRE(paths.size() == names.size());

    REQUIRE(TileStyleName(TileStyle::kCycle) == "OpenCycleMap");
    REQUIRE(TileUrl(TileStyle::kMobileAtlas, {1, 2, 10}, "key") ==
            "https://tile.thunderforest.com/mobile-atlas/10/1/2.png?apikey=key");
    REQUIRE(TilePath(TileStyle::kMobileAtlas, {1, 2, 10}) == "tiles/mobatlas/10/1/2.png");
    REQUIRE(RegionsPath(TileStyle::kLandscape) == "pending/landscap/regions.bin");
}

TEST_CASE("unstyled tiles are migrated to the cycle directory")
{
    mock_filesystem::Clear();
    Filesystem filesystem("");

    GIVEN("tiles and regions from before the style directory")
    {
        mock_filesystem::SetFile("tiles/15/17600/9300.png", "z15");
        mock_filesystem::SetFile("tiles/15/17601/9300.png", "z15 again");
        mock_filesystem::SetFile("tiles/13/4400/2325.png", "z13");
        mock_filesystem::SetFile("tiles/10/550/290.png", "z10");
        mock_filesystem::SetFile("pending/regions.bin", "regions");
        mock_filesystem::SetFile("OSM_KEY.TXT", "key");

        WHEN("migrating")
        {
            MigrateUnstyledTiles(filesystem);

            THEN("everything is in the cycle directory")
            {
                REQUIRE(Contents(filesystem, "tiles/cycle/15/17600/9300.png") == "z15");
                REQUIRE(Contents(filesystem, "tiles/cycle/15/17601/9300.png") == "z15 again");
                REQUIRE(Contents(filesystem, "tiles/cycle/13/4400/2325.png") == "z13");
                REQUIRE(Contents(filesystem, "tiles/cycle/10/550/290.png") == "z10");
                REQUIRE(Contents(filesystem, "pending/cycle/regions.bin") == "regions");

                REQUIRE_FALSE(filesystem.FileExists("tiles/15/17600/9300.png"));
                REQUIRE_FALSE(filesystem.FileExists("tiles/13/4400/2325.png"));
                REQUIRE_FALSE(filesystem.FileExists("tiles/10/550/290.png"));
                REQUIRE_FALSE(filesystem.FileExists("pending/regions.bin"));
            }

            AND_THEN("unrelated files are left alone")
            {
                REQUIRE(Contents(filesystem, "OSM_KEY.TXT") == "key");
            }

            AND_WHEN("migrating again (i.e., the next boot)")
            {
                MigrateUnstyledTiles(filesystem);

                THEN("nothing changes")
                {
                    REQUIRE(Contents(filesystem, "tiles/cycle/15/17600/9300.png") == "z15");
                    REQUIRE(Contents(filesystem, "pending/cycle/regions.bin") == "regions");
                }
            }
        }
    }

    GIVEN("a migration which was interrupted after the first zoom level")
    {
        mock_filesystem::SetFile("tiles/cycle/15/17600/9300.png", "z15");
        mock_filesystem::SetFile("tiles/13/4400/2325.png", "z13");

        WHEN("migrating")
        {
            MigrateUnstyledTiles(filesystem);

            THEN("the rest is moved")
            {
                REQUIRE(Contents(filesystem, "tiles/cycle/15/17600/9300.png") == "z15");
                REQUIRE(Contents(filesystem, "tiles/cycle/13/4400/2325.png") == "z13");
                REQUIRE_FALSE(filesystem.FileExists("tiles/13/4400/2325.png"));
            }
        }
    }

    GIVEN("both old and new tiles for the same zoom level")
    {
        mock_filesystem::SetFile("tiles/15/17600/9300.png", "old");
        mock_filesystem::SetFile("tiles/cycle/15/17600/9300.png", "new");
        mock_filesystem::SetFile("pending/regions.bin", "old regions");
        mock_filesystem::SetFile("pending/cycle/regions.bin", "new regions");

        WHEN("migrating")
        {
            MigrateUnstyledTiles(filesystem);

            THEN("the new ones are not overwritten")
            {
                REQUIRE(Contents(filesystem, "tiles/cycle/15/17600/9300.png") == "new");
                REQUIRE(Contents(filesystem, "pending/cycle/regions.bin") == "new regions");
            }
        }
    }

    GIVEN("nothing on disk")
    {
        WHEN("migrating")
        {
            MigrateUnstyledTiles(filesystem);

            THEN("nothing is created")
            {
                REQUIRE_FALSE(filesystem.FileExists("pending/cycle/regions.bin"));
            }
        }
    }

    mock_filesystem::Clear();
}

TEST_SUITE_END();
