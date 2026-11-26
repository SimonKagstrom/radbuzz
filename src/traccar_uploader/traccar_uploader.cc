#include "traccar_uploader.hh"

#include "fnv1a_hash.hh"
#include "json_helpers.hh"
#include "split_string.hh"
#include "trip_compactor.hh"
#include "trip_recorder.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>
#include <iterator>
#include <limits>
#include <nlohmann/json.hpp>
#include <span>

namespace
{

constexpr auto kKmhPerKnot = 1.852f;

// Unreserved characters as they are, everything else percent-encoded
std::string
UrlEncode(std::string_view value)
{
    std::string out;
    for (auto ch : value)
    {
        if (std::isalnum(static_cast<unsigned char>(ch)) ||
            std::string_view("-._~").find(ch) != std::string_view::npos)
        {
            out += ch;
        }
        else
        {
            out += std::format("%{:02X}", static_cast<uint8_t>(ch));
        }
    }

    return out;
}

// The UTC of a parsed trip line, or nothing if the line is broken. There are no exceptions on
// target, so the types are checked before getting a value
std::optional<uint32_t>
GetUtc(const nlohmann::json& json)
{
    if (!json.is_object())
    {
        return std::nullopt;
    }

    auto utc = json.find("utc");
    if (utc == json.end() || !utc->is_number_unsigned() ||
        utc->get<uint64_t>() > std::numeric_limits<uint32_t>::max())
    {
        return std::nullopt;
    }

    return utc->get<uint32_t>();
}

// The lines after the one with the UTC, or all of them if it isn't in the trip
std::span<const std::string_view>
LinesAfter(std::span<const std::string_view> lines, uint32_t last_sent_utc)
{
    const auto it = std::ranges::find_if(lines, [last_sent_utc](auto line) {
        return GetUtc(nlohmann::json::parse(line, nullptr, false)) == last_sent_utc;
    });

    return it == lines.end() ? lines : std::span(std::next(it), lines.end());
}

std::string
PathIn(std::string_view root, std::string_view directory, std::string_view name)
{
    return std::format("{}/{}", TripRecorder::Directory(root, directory), name);
}

} // namespace

TraccarUploader::TraccarUploader(ApplicationState& state,
                                 PostOffice<MSG::AllMessages>& post_office,
                                 Filesystem& filesystem,
                                 HttpsClient& https_client)
    : m_state(state)
    , m_filesystem(filesystem)
    , m_https_client(https_client)
    , m_listener(m_state.AttachListener<AS::wifi_connected>(GetSemaphore()))
    , m_mailbox(post_office.Subscribe<MSG::new_compacted_trip>(GetSemaphore()))
{
}

void
TraccarUploader::OnStartup()
{
    ImportConfiguration();
}

std::optional<milliseconds>
TraccarUploader::OnActivation()
{
    // Only wakes up, the log directories are walked below
    m_mailbox->Collect();

    if (!m_state.Get<AS::wifi_connected>())
    {
        // Uploaded when connected again
        return std::nullopt;
    }

    const auto all_uploaded = UploadAll();
    m_state.Set<AS::traccar::upload_active>(false);

    if (all_uploaded)
    {
        return std::nullopt;
    }

    return kRetryTime;
}

void
TraccarUploader::ImportConfiguration()
{
    auto data = m_filesystem.ReadFile(kConfigurationFile);
    if (!data)
    {
        // Keep the configuration from the NVM
        return;
    }

    // Only import a changed file, like ssid.txt
    const auto hash = Fnv1aHash(*data);
    if (hash == m_state.Get<AS::traccar::configuration_file_hash>())
    {
        return;
    }

    auto json = ParseJson(*data);
    if (!json.is_object())
    {
        printf("TraccarUploader: %s isn't a JSON object, ignoring it\n", kConfigurationFile);
        return;
    }

    auto server = GetJsonString(json, "url");
    // Without the trailing slash, which is added for each request
    while (server.ends_with('/'))
    {
        server.pop_back();
    }

    auto qw = m_state.CheckoutQueuedWriter<AS::traccar>();

    qw.Set<AS::traccar::configuration>({
        .url = server,
        .id = GetJsonString(json, "id"),
        .demo_id = GetJsonString(json, "demo_id"),
    });
    qw.Set<AS::traccar::configuration_file_hash>(hash);
    // A new server (or device) needs a partly uploaded trip from the start
    qw.Set<AS::traccar::last_sent_utc>(0);

    printf("TraccarUploader: imported %s\n", kConfigurationFile);
}

bool
TraccarUploader::UploadAll()
{
    const auto traccar = m_state.Get<AS::traccar::configuration>();
    if (traccar->url.empty())
    {
        return true;
    }

    for (const auto& [root, device_id] : {std::pair {TripRecorder::kTripsRoot, traccar->id},
                                          std::pair {TripRecorder::kDemoRoot, traccar->demo_id}})
    {
        if (device_id.empty())
        {
            continue;
        }

        for (const auto& entry : m_filesystem.ListDirectory(
                 TripRecorder::Directory(root, TripCompactor::kLogDirectory)))
        {
            // Stop at the first failure, since the server is probably unreachable
            if (!entry.is_directory && !Upload(root, device_id, entry.name))
            {
                return false;
            }
        }
    }

    return true;
}

bool
TraccarUploader::Upload(std::string_view root, std::string_view device_id, std::string_view name)
{
    const auto log_path = PathIn(root, TripCompactor::kLogDirectory, name);

    auto data = m_filesystem.ReadFile(log_path);
    if (!data)
    {
        // Removed since it was listed
        return true;
    }

    // Read at once, since compacted trips have a bounded size
    const auto lines =
        SplitLines(std::string_view(reinterpret_cast<const char*>(data->data()), data->size()));
    auto unsent = LinesAfter(lines, m_state.Get<AS::traccar::last_sent_utc>());

    const auto server = m_state.Get<AS::traccar::configuration>()->url;

    std::optional<uint32_t> last_sent_utc;
    auto sent = 0u;
    for (auto line : unsent)
    {
        auto request = CreatePointRequest(server, device_id, line);
        if (!request)
        {
            // Skip broken lines
            continue;
        }

        m_state.Set<AS::traccar::upload_active>(true);
        if (!m_https_client.Get(request->url))
        {
            // Continue with this point next time. If it did arrive after all, it's the newest
            // point, which Traccar filters
            if (last_sent_utc)
            {
                m_state.Set<AS::traccar::last_sent_utc>(*last_sent_utc);
            }
            return false;
        }

        last_sent_utc = request->utc;
        sent++;
        if (sent % kProgressInterval == 0)
        {
            m_state.Set<AS::traccar::last_sent_utc>(*last_sent_utc);
        }
    }
    // Nothing is sent again if the trip can't be moved
    if (last_sent_utc)
    {
        m_state.Set<AS::traccar::last_sent_utc>(*last_sent_utc);
    }

    // Done, so that only pending trips are left in the log directory. A trip with the same name
    // is replaced, e.g., if it was copied back to be uploaded again
    const auto sent_path = PathIn(root, kSentDirectory, name);
    if (m_filesystem.FileExists(sent_path))
    {
        m_filesystem.Remove(sent_path);
    }
    if (!m_filesystem.Move(log_path, sent_path))
    {
        // Tried again on the next upload
        printf("TraccarUploader: can't move %s to %s\n", log_path.c_str(), sent_path.c_str());
        return true;
    }

    // So that the trip is sent from the start, if it's moved back
    m_state.Set<AS::traccar::last_sent_utc>(0);

    return true;
}

std::optional<TraccarUploader::PointRequest>
TraccarUploader::CreatePointRequest(std::string_view server,
                                    std::string_view device_id,
                                    std::string_view line)
{
    auto json = nlohmann::json::parse(line, nullptr, false);
    auto utc = GetUtc(json);
    if (!utc)
    {
        return std::nullopt;
    }

    auto latitude = json.find("lat");
    auto longitude = json.find("lon");
    if (latitude == json.end() || !latitude->is_number() || longitude == json.end() ||
        !longitude->is_number())
    {
        return std::nullopt;
    }

    auto out = std::format("{}/?id={}&timestamp={}&lat={:.6f}&lon={:.6f}",
                           server,
                           UrlEncode(device_id),
                           *utc,
                           latitude->get<double>(),
                           longitude->get<double>());

    // Recorded in km/h. Traccar reads OsmAnd speeds in knots
    if (auto speed = json.find("speed"); speed != json.end() && speed->is_number())
    {
        out += std::format("&speed={:.1f}", speed->get<float>() / kKmhPerKnot);
    }
    if (auto soc = json.find("soc"); soc != json.end() && soc->is_number_integer())
    {
        out += std::format("&batt={}", soc->get<int64_t>());
    }
    // Not part of the protocol, so stored as an attribute by Traccar. Not as "power", which is the
    // supply voltage there
    if (auto power = json.find("power"); power != json.end() && power->is_number_integer())
    {
        out += std::format("&watts={}", power->get<int64_t>());
    }

    return PointRequest {*utc, std::move(out)};
}
