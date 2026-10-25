#pragma once

#include "configuration_settings.hh"
#include "filesystem.hh"
#include "wgs84_to_osm_point.hh"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

// The map styles (TileStyle), all from Thunderforest

// Shown to the user
std::string_view TileStyleName(TileStyle style);

// A sample tile (PNG) of the style, to show the user
std::span<const std::byte> TileStyleSample(TileStyle style);

// https://tile.thunderforest.com/<style>/<zoom>/<x>/<y>.png?apikey=<api_key>
std::string TileUrl(TileStyle style, const Tile& tile, std::string_view api_key);

// tiles/<style>/<zoom>/<x>/<y>.png
std::string TilePath(TileStyle style, const Tile& tile);

// pending/<style>/regions.bin
std::string RegionsPath(TileStyle style);

// Tiles and regions used to be stored without the style directory, and were all OpenCycleMap
// tiles. Move them into the cycle directory. Does nothing if already done
void MigrateUnstyledTiles(const Filesystem& filesystem);
