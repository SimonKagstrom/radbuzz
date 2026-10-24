// The list of all messages for radbuzz
#pragma once

#include "common_messages.hh"
#include "message_helpers.hh"
#include "radbuzz_messages.hh"

#include <cstddef>
#include <cstdint>

namespace MSG
{

using AllMessages = ConcatMessages<CommonMessages, RadbuzzMessages>;

constexpr auto kMessageCount = std::tuple_size_v<AllMessages>;

template <typename T>
consteval uint8_t
IndexOf()
{
    return detail::IndexOfImpl<T, AllMessages>::Get();
}

} // namespace MSG
