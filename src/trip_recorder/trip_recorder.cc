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
    , m_post_office(post_office)
    , m_mailbox(m_post_office.Subscribe<MSG::reset_trip>(GetSemaphore()))
{
    m_buffer.reserve(kBufferSize + kMaxLineSize);
}

std::string
TripRecorder::Directory(std::string_view root, std::string_view directory)
{
    return std::format("{}/{}", root, directory);
}

void
TripRecorder::OnStartup()
{
    // A trip left from before a power-off is closed, the same as on a reset
    const auto current = Directory(kTripsRoot, kCurrentDirectory);
    const auto raw = Directory(kTripsRoot, kRawDirectory);
    for (const auto& entry : m_filesystem.ListDirectory(current))
    {
        if (entry.is_directory)
        {
            continue;
        }

        if (m_filesystem.Move(std::format("{}/{}", current, entry.name),
                              std::format("{}/{}", raw, entry.name)))
        {
            m_post_office.Send<MSG::new_recorded_trip>();
        }
        else
        {
            printf("TripRecorder: can't move %s to %s\n", entry.name.c_str(), raw.c_str());
        }
    }

    // A demo trip is only kept on a reset, so that it's a deliberate choice
    const auto demo_current = Directory(kDemoRoot, kCurrentDirectory);
    for (const auto& entry : m_filesystem.ListDirectory(demo_current))
    {
        if (!entry.is_directory &&
            !m_filesystem.Remove(std::format("{}/{}", demo_current, entry.name)))
        {
            printf("TripRecorder: can't remove demo trip %s\n", entry.name.c_str());
        }
    }
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
                // Left in current, so the next trip still starts in a new file
                printf("TripRecorder: can't move %s to %s\n",
                       m_filename.c_str(),
                       Directory(m_root, kRawDirectory).c_str());
            }
            else
            {
                m_post_office.Send<MSG::new_recorded_trip>();
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
    if (!m_state.Get<AS::is_moving>())
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
        // Demo mode changes send a trip reset, so the whole trip is in the same mode
        m_root = m_state.Get<AS::demo_mode>() ? kDemoRoot : kTripsRoot;
        m_filename = std::format("{:08x}.jsl", utc);
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
    return std::format("{}/{}/{}", m_root, directory, m_filename);
}
