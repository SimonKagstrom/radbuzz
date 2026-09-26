#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

class GadgetBridgeProtocol
{
public:
    // Handle a complete line (without the newline) from Gadgetbridge
    void PushLine(std::string_view line);

    // Parse a GB({...}) line into json, or nullopt if it's not a (valid) GB message
    static std::optional<nlohmann::json> ParseLine(std::string_view line);
};
