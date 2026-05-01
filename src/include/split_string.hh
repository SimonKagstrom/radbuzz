#pragma once

#include <sstream>
#include <string>
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
