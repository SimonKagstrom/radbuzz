#pragma once

#include <optional>
#include <string>
#include <string_view>

class LineCollector
{
public:
    std::optional<std::string> Push(std::string_view line)
    {
        if (line.empty())
        {
            return std::nullopt;
        }

        m_buffer += line;
        if (m_buffer.back() == '\n')
        {
            std::string complete_line = std::move(m_buffer);

            m_buffer.clear();

            return complete_line;
        }

        return std::nullopt;
    }

private:
    std::string m_buffer;
};