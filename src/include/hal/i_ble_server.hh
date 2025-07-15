#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace hal
{

namespace detail
{

// TODO: Cleanup this messy hack
constexpr std::array<uint8_t, 16>
StringToUuid128(std::string_view string)
{
    std::array<uint8_t, 16> out = {};

    auto index = 15;
    for (auto i = 0u; i < string.size(); i += 2)
    {
        if (string[i] == '-')
        {
            i++;
        }

        uint8_t v = 0;
        for (auto j = 0; j < 2; j++)
        {
            auto c = string[i + j];
            if (c >= '0' && c <= '9')
            {
                v = v + ((c - '0') << ((1 - j) * 4));
            }
            else
            {
                v = v + ((c - 'a' + 10) << ((1 - j) * 4));
            }
        }

        out[index] = v;
        index--;
    }

    return out;
}

} // namespace detail

class IBleServer
{
public:
    virtual ~IBleServer() = default;

    virtual void SetServiceUuid128(std::span<const uint8_t, 16> service_uuid) = 0;

    virtual void
    AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                std::function<void(std::span<const uint8_t>)> data) = 0;

    /** @brief Start the BLE server
     */
    virtual void Start() = 0;

    /**
     * Poll BLE events.
     * 
     * Should be called periodically
     */
    virtual void PollEvents() = 0;

protected:
};

} // namespace hal
