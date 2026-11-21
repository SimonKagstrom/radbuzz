#include "trip_recorder.hh"

#include <cstdio>
#include <format>

namespace
{

// More than a line, so that a full buffer doesn't need to grow
constexpr auto kMaxLineSize = 256uz;

} // namespace

TripRecorder::TripRecorder(ApplicationState& state,
                           PostOffice<MSG::AllMessages>& post_office,
                           Filesystem& filesystem)
    : m_state(state)
    , m_filesystem(filesystem)
    , m_listener(m_state.AttachListener<AS::pixel_position, AS::is_moving>(GetSemaphore()))
    , m_state_cache(m_state)
    , m_mailbox(post_office.Subscribe<MSG::reset_trip>(GetSemaphore()))
{
    m_buffer.reserve(kBufferSize + kMaxLineSize);
}

std::optional<milliseconds>
TripRecorder::OnActivation()
{
    m_mailbox->Collect().On<MSG::reset_trip>([this](auto) {
        if (!m_filename.empty())
        {
            Flush();
            if (!m_filesystem.Move(PathIn(kCurrentDirectory), PathIn(kRawDirectory)))
            {
                // Left in CURRENT, so the next trip still starts in a new file
                printf("TripRecorder: can't move %s to %s\n", m_filename.c_str(), kRawDirectory);
            }

            m_filename.clear();
        }
    });

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

    if (m_filename.empty())
    {
        m_filename = std::format("{:08X}.JSL", utc);
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

    auto file = m_filesystem.OpenWritable(PathIn(kCurrentDirectory));
    if (!file || !file->Write(std::as_bytes(std::span(m_buffer))) || !file->Sync())
    {
        // Dropped, so that the buffer doesn't grow without bounds
        printf("TripRecorder: can't write %s, dropping %zu bytes\n",
               m_filename.c_str(),
               m_buffer.size());
    }

    m_buffer.clear();
}

std::string
TripRecorder::PathIn(std::string_view directory) const
{
    return std::format("{}/{}", directory, m_filename);
}
