#pragma once

#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace detail
{
// http://stackoverflow.com/questions/236129/how-to-split-a-string-in-c
inline void
Split(const std::string& s, char delim, std::vector<std::string>& elems)
{
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
}

} // namespace detail


inline std::vector<std::string>
SplitString(const std::string& s, std::string_view delims)
{
    std::vector<std::string> elems;
    for (auto c : delims)
    {
        detail::Split(s, c, elems);
    }

    return elems;
}

/// The non-empty lines of a text, without the newlines. Views into the text
inline std::vector<std::string_view>
SplitLines(std::string_view text)
{
    std::vector<std::string_view> out;
    for (auto line : std::views::split(text, '\n'))
    {
        if (!line.empty())
        {
            out.emplace_back(line.begin(), line.end());
        }
    }

    return out;
}
