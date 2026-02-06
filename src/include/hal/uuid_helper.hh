#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace hal
{

using Uuid128 = std::array<uint8_t, 16>;
using Uuid128Span = std::span<const uint8_t, 16>;

namespace detail
{

/// Converts a UUID string to a 16-byte array in little-endian format.
/// @param uuid_string UUID string in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
/// @return 16-byte array representation of the UUID
constexpr Uuid128
StringToUuid128(std::string_view uuid_string)
{
    Uuid128 result = {};

    auto hex_char_to_value = [](char c) constexpr -> uint8_t {
        c = std::tolower(c);
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;

        return 0;
    };

    size_t byte_index = 15; // Start from end (little-endian)

    for (size_t i = 0; i < uuid_string.size() && byte_index < 16; ++i)
    {
        char c = uuid_string[i];

        // Skip hyphens
        if (c == '-')
            continue;

        // Parse two hex characters into one byte
        uint8_t high_nibble = hex_char_to_value(c);
        uint8_t low_nibble = (i + 1 < uuid_string.size()) ? hex_char_to_value(uuid_string[++i]) : 0;

        result[byte_index--] = (high_nibble << 4) | low_nibble;
    }

    return result;
}
} // namespace detail

} // namespace hal