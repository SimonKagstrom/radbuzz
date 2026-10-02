// The list of all messages for radbuzz
#pragma once

#include "message_helpers.hh"

#include <cstddef>
#include <string>

namespace MSG
{

struct incoming_call
{
    std::string caller_number;
    std::string caller_name;
};

struct tile_loaded
{
};

struct reset_trip
{
};

using AllMessages = std::tuple<incoming_call, tile_loaded, reset_trip>;

constexpr auto kMessageCount = std::tuple_size_v<AllMessages>;

template <typename T>
consteval uint8_t
IndexOf()
{
    return detail::IndexOfImpl<T, AllMessages>::Get();
}

} // namespace MSG
