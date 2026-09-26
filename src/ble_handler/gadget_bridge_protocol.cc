#include "gadget_bridge_protocol.hh"

#include <cctype>
#include <cstdio>
#include <string>

namespace
{

constexpr std::string_view kPrefix = "GB(";
constexpr std::string_view kSuffix = ")";

std::string_view
Trim(std::string_view s)
{
    // Gadgetbridge prefixes lines with \x10 (echo off), and sometimes sends \x03 (Ctrl-C)
    auto is_space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    auto is_junk = [&](char c) { return c == '\x10' || c == '\x03' || is_space(c); };

    while (!s.empty() && is_junk(s.front()))
    {
        s.remove_prefix(1);
    }
    while (!s.empty() && (is_space(s.back()) || s.back() == ';'))
    {
        s.remove_suffix(1);
    }

    return s;
}

// Gadgetbridge sends JavaScript, which can contain \xHH escapes that aren't valid JSON
std::string
JsToJson(std::string_view s)
{
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            if (s[i + 1] == 'x')
            {
                out += "\\u00";
            }
            else
            {
                out += s[i];
                out += s[i + 1];
            }
            i++;
            continue;
        }

        out += s[i];
    }

    return out;
}

} // namespace

std::optional<nlohmann::json>
GadgetBridgeProtocol::ParseLine(std::string_view line)
{
    line = Trim(line);

    if (!line.starts_with(kPrefix) || !line.ends_with(kSuffix))
    {
        return std::nullopt;
    }
    line.remove_prefix(kPrefix.size());
    line.remove_suffix(kSuffix.size());

    // No exceptions on target, so use the non-throwing parser
    auto json = nlohmann::json::parse(JsToJson(line), nullptr, false);
    if (json.is_discarded())
    {
        return std::nullopt;
    }

    return json;
}

void
GadgetBridgeProtocol::PushLine(std::string_view line)
{
    auto json = ParseLine(line);

    if (!json)
    {
        printf("GB: ignoring line: %.*s\n", static_cast<int>(line.size()), line.data());
        return;
    }

    // Replace invalid UTF-8, since dump() would otherwise throw (i.e., abort)
    auto str = json->dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    printf("GB: %s\n", str.c_str());
}
