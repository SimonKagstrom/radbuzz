#pragma once

#include "image.hh"

#include <cstddef>
#include <span>

// Decode a PNG to RGB565. Fails if it doesn't have exactly the same size as out
bool DecodePng(std::span<const std::byte> png_data, StandaloneImage& out);
