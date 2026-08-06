#pragma once

#include <cstdint>

struct BmsData
{
    bool valid;
    uint8_t soc;
    uint8_t highest_cell_temp;
    uint8_t bms_temperature;

    bool operator==(const BmsData& other) const = default;
};
