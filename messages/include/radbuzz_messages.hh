// The messages specific to radbuzz
#pragma once

#include <cstdint>
#include <string>
#include <tuple>

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

// Answered, here or on the phone
struct call_started
{
};

struct message
{
    uint32_t id;
    std::string source;
    std::string title;
    std::string body;
};

// Phone dismisses message
struct dismiss_message
{
    uint32_t id;
};

// User dismisses message here, to dismiss it on the phone as well
struct user_dismissed_message
{
    uint32_t id;
};

// Show the about box (version, map attribution)
struct show_about
{
};

using RadbuzzMessages = std::tuple<incoming_call,
                                   tile_loaded,
                                   reset_trip,
                                   answer_call,
                                   decline_call,
                                   hangup_call,
                                   call_ended,
                                   call_started,
                                   message,
                                   dismiss_message,
                                   user_dismissed_message,
                                   show_about>;

} // namespace MSG
