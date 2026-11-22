#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

/// 32-bit FNV-1a, e.g., to detect changes to configuration files
inline uint32_t
Fnv1aHash(std::span<const std::byte> data)
{
    uint32_t hash = 2166136261u;

    for (auto byte : data)
    {
        hash ^= std::to_integer<uint32_t>(byte);
        hash *= 16777619u;
    }

    return hash;
}
