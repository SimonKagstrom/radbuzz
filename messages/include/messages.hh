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

struct answer_call
{
};

struct decline_call
{
};

struct hangup_call
{
};

struct call_ended
{
};

using AllMessages = std::tuple<incoming_call,
                               tile_loaded,
                               reset_trip,
                               answer_call,
                               decline_call,
                               hangup_call,
                               call_ended>;

constexpr auto kMessageCount = std::tuple_size_v<AllMessages>;

template <typename T>
consteval uint8_t
IndexOf()
{
    return detail::IndexOfImpl<T, AllMessages>::Get();
}

} // namespace MSG
