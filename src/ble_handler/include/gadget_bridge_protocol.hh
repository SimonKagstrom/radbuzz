#pragma once

#include <nlohmann/json.hpp>
#include <string_view>

class GadgetBridgeProtocol
{
public:
    void PushLine(std::string_view line);

private:
};
