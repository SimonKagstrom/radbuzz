#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>

// Without exceptions on target, so parse errors are discarded values and types are checked

/// Parse file contents as JSON. A broken file gives a discarded value (not an object)
inline nlohmann::json
ParseJson(std::span<const std::byte> data)
{
    const auto text = std::string_view(reinterpret_cast<const char*>(data.data()), data.size());

    return nlohmann::json::parse(text, nullptr, false);
}

/// The string value of a key, or empty if missing or not a string (or json isn't an object)
inline std::string
GetJsonString(const nlohmann::json& json, const char* key)
{
    if (!json.is_object())
    {
        return {};
    }

    auto it = json.find(key);
    return it != json.end() && it->is_string() ? it->get<std::string>() : std::string();
}
