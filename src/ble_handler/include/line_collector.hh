#pragma once

#include <optional>
#include <string>
#include <string_view>

class LineCollector
{
public:
    // Guard against a peer which never sends a newline
    static constexpr auto kMaxLineLength = 4096;

    void Push(std::string_view data)
    {
        m_buffer += data;

        if (m_buffer.size() > kMaxLineLength && m_buffer.find('\n') == std::string::npos)
        {
            m_buffer.clear();
        }
    }

    // Return the next complete line (without the newline), if any
    std::optional<std::string> Poll()
    {
        auto newline = m_buffer.find('\n');
        if (newline == std::string::npos)
        {
            return std::nullopt;
        }

        auto line = m_buffer.substr(0, newline);
        m_buffer.erase(0, newline + 1);

        return line;
    }

private:
    std::string m_buffer;
};
