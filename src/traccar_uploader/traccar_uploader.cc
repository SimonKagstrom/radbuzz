#include "traccar_uploader.hh"

#include "fnv1a_hash.hh"
#include "split_string.hh"
#include "trip_compactor.hh"
#include "trip_recorder.hh"

#include <cstdio>
#include <format>
#include <nlohmann/json.hpp>

namespace
{

constexpr auto kKmhPerKnot = 1.852f;

// The string value of a key, or empty if missing or not a string
std::string
GetString(const nlohmann::json& json, const char* key)
{
    auto it = json.find(key);
    return it != json.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// Unreserved characters as they are, everything else percent-encoded
std::string
UrlEncode(std::string_view value)
{
    std::string out;
    for (auto ch : value)
    {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '.' || ch == '_' || ch == '~')
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

// The OsmAnd request for a trip line, or nothing if the line is broken
std::optional<std::string>
PointUrl(std::string_view server, std::string_view device_id, std::string_view line)
{
    // No exceptions on target, so check every type before getting a value
    auto json = nlohmann::json::parse(line, nullptr, false);
    if (!json.is_object())
    {
        return std::nullopt;
    }

    auto utc = json.find("utc");
    auto latitude = json.find("lat");
    auto longitude = json.find("lon");
    if (utc == json.end() || !utc->is_number_unsigned() || latitude == json.end() ||
        !latitude->is_number() || longitude == json.end() || !longitude->is_number())
    {
        return std::nullopt;
    }

    auto out = std::format("{}/?id={}&timestamp={}&lat={:.6f}&lon={:.6f}",
                           server,
                           UrlEncode(device_id),
                           utc->get<uint64_t>(),
                           latitude->get<double>(),
                           longitude->get<double>());

    // Recorded in km/h. Traccar reads OsmAnd speeds in knots (to be verified with the server)
    if (auto speed = json.find("speed"); speed != json.end() && speed->is_number())
    {
        out += std::format("&speed={:.1f}", speed->get<double>() / kKmhPerKnot);
    }
    if (auto soc = json.find("soc"); soc != json.end() && soc->is_number_integer())
    {
        out += std::format("&batt={}", soc->get<int64_t>());
    }
    // Not part of the protocol, so stored as an attribute by Traccar
    if (auto power = json.find("power"); power != json.end() && power->is_number_integer())
    {
        out += std::format("&power={}", power->get<int64_t>());
    }

    return out;
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
    m_mailbox->Collect().On<MSG::new_compacted_trip>([](auto) {});

    if (!m_state.Get<AS::wifi_connected>())
    {
        // Uploaded when connected again
        return std::nullopt;
    }

    if (UploadAll())
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

    // Only import a changed file, like SSID.TXT
    const auto hash = Fnv1aHash(*data);
    if (hash == m_state.Get<AS::configuration>()->traccar_file_hash)
    {
        return;
    }

    const auto text = std::string_view(reinterpret_cast<const char*>(data->data()), data->size());
    auto json = nlohmann::json::parse(text, nullptr, false);
    if (!json.is_object())
    {
        printf("TraccarUploader: %s isn't a JSON object, ignoring it\n", kConfigurationFile);
        return;
    }

    auto server = GetString(json, "url");
    // Without the trailing slash, which is added for each request
    while (server.ends_with('/'))
    {
        server.pop_back();
    }

    auto ps = m_state.CheckoutPartialSnapshot<AS::configuration>();
    auto& conf = ps.GetWritableReference<AS::configuration>();

    conf.traccar = {
        .url = server,
        .id = GetString(json, "id"),
        .demo_id = GetString(json, "demo_id"),
    };
    conf.traccar_file_hash = hash;

    printf("TraccarUploader: imported %s\n", kConfigurationFile);
}

bool
TraccarUploader::UploadAll()
{
    const auto traccar = m_state.Get<AS::configuration>()->traccar;
    if (traccar.url.empty())
    {
        return true;
    }

    for (const auto& [root, device_id] : {std::pair {TripRecorder::kTripsRoot, traccar.id},
                                          std::pair {TripRecorder::kDemoRoot, traccar.demo_id}})
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
    const auto server = m_state.Get<AS::configuration>()->traccar.url;
    for (auto line :
         SplitLines(std::string_view(reinterpret_cast<const char*>(data->data()), data->size())))
    {
        auto url = PointUrl(server, device_id, line);

        // Broken lines are skipped. A failed trip is sent again from the start, and Traccar
        // filters the duplicates
        if (url && !m_https_client.Get(*url))
        {
            return false;
        }
    }

    // Done, so that only pending trips are left in the log directory
    const auto sent_path = PathIn(root, kSentDirectory, name);
    if (!m_filesystem.Move(log_path, sent_path))
    {
        // Uploaded again on the next upload
        printf("TraccarUploader: can't move %s to %s\n", log_path.c_str(), sent_path.c_str());
    }

    return true;
}
