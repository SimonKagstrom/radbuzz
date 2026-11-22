#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "https_client.hh"
#include "messages.hh"
#include "post_office.hh"

#include <string>
#include <string_view>

/**
 * @brief Uploads compacted trips to a Traccar server, with the OsmAnd protocol
 *
 * At startup, traccar.txt is imported into the configuration (and thereby the NVM), if it has
 * changed since the last import. It's JSON, with the server url, the device id for real trips,
 * and optionally the device id for demo trips:
 *
 *   {"url": "https://host.tailnet.ts.net", "id": "k3v9x2qmw7rt", "demo_id": "p8d4n6hzc1ya"}
 *
 * While wifi is connected, every line of the trips in <root>/log is sent as one request. A
 * completely uploaded trip is moved to <root>/sent, and an interrupted upload starts over
 * (Traccar filters the duplicates). Nothing is uploaded without an id, and demo trips not
 * without a demo id.
 */
class TraccarUploader : public os::BaseThread
{
public:
    static constexpr auto kConfigurationFile = "traccar.txt";
    static constexpr auto kSentDirectory = "sent";
    /// Time until a failed upload is retried, while wifi is connected
    static constexpr auto kRetryTime = milliseconds(5min);

    TraccarUploader(ApplicationState& state,
                    PostOffice<MSG::AllMessages>& post_office,
                    Filesystem& filesystem,
                    HttpsClient& https_client);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void ImportConfiguration();

    /// Upload every pending trip, true if there's nothing left
    bool UploadAll();

    /// Upload a trip from <root>/log, true if it's completely uploaded
    bool Upload(std::string_view root, std::string_view device_id, std::string_view name);

    ApplicationState& m_state;
    Filesystem& m_filesystem;
    HttpsClient& m_https_client;
    ListenerCookie m_listener;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
};
