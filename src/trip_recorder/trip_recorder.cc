#include "trip_recorder.hh"

#include <cstdio>
#include <format>

namespace
{

// More than a line, so that a full buffer doesn't need to grow
constexpr auto kMaxLineSize = 256uz;

} // namespace

TripRecorder::TripRecorder(ApplicationState& state, Filesystem& filesystem)
    : m_state(state)
    , m_filesystem(filesystem)
    , m_listener(m_state.AttachListener<AS::pixel_position, AS::is_moving>(GetSemaphore()))
    , m_state_cache(m_state)
{
    m_buffer.reserve(kBufferSize + kMaxLineSize);
}

std::optional<milliseconds>
TripRecorder::OnActivation()
{
    // The pixel position is less noisy than the GPS position, and changes only when moving at
    // least a few meters. Written together with the position and time by the GPS reader
    m_state_cache.Pull()
        .OnChanged<AS::pixel_position>([this]() { Record(*m_state.Get<AS::position>()); })
        .OnNewValue<AS::is_moving>([this](auto is_moving) {
            if (!is_moving)
            {
                Flush();
            }
        });

    return std::nullopt;
}

void
TripRecorder::Record(const GpsData& data)
{
    if (m_state.Get<AS::demo_mode>() || !m_state.Get<AS::is_moving>())
    {
        return;
    }

    auto utc = m_state.Get<AS::gps_utc>();
    if (utc == 0 || utc == m_last_utc)
    {
        return;
    }
    m_last_utc = utc;

    if (m_path.empty())
    {
        m_path = std::format("{}/{:08X}.JSL", kCurrentDirectory, utc);
    }

    std::format_to(std::back_inserter(m_buffer),
                   R"({{"utc":{},"lat":{:.6f},"lon":{:.6f},"speed":{:.1f},"power":{},"soc":{}}})"
                   "\n",
                   utc,
                   data.position.latitude,
                   data.position.longitude,
                   data.speed,
                   m_state.Get<AS::current_power_w>(),
                   m_state.Get<AS::battery_soc>());

    if (m_buffer.size() >= kBufferSize)
    {
        Flush();
    }
}

void
TripRecorder::Flush()
{
    if (m_buffer.empty())
    {
        return;
    }

    auto file = m_filesystem.OpenWritable(m_path);
    if (!file || !file->Write(std::as_bytes(std::span(m_buffer))) || !file->Sync())
    {
        // Dropped, so that the buffer doesn't grow without bounds
        printf("TripRecorder: can't write %s, dropping %zu bytes\n", m_path.c_str(), m_buffer.size());
    }

    m_buffer.clear();
}
