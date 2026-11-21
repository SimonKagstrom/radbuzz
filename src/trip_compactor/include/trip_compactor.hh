#pragma once

#include "filesystem.hh"
#include "messages.hh"
#include "pooled_thread_base.hh"
#include "post_office.hh"

#include <memory>
#include <string_view>

/**
 * @brief Compacts recorded trips, so that they have a bounded size
 *
 * On MSG::new_recorded_trip, every trip in TRIPS/RAW is simplified to at most kMaxEntries
 * points with the TripLog, and the kept lines are copied unchanged to TRIPS/LOG, with the same
 * name. The raw trip is then removed. Lines which can't be parsed are skipped. A trip which
 * can't be written is left in TRIPS/RAW, and is retried on the next message.
 */
class TripCompactor : public PooledThreadBase
{
public:
    static constexpr auto kLogDirectory = "TRIPS/LOG";
    static constexpr auto kMaxEntries = 1000uz;

    TripCompactor(PostOffice<MSG::AllMessages>& post_office, Filesystem& filesystem);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    /// Compact and remove every trip in TRIPS/RAW
    void WalkRawDirectory();

    /// Compact a trip from TRIPS/RAW to TRIPS/LOG, true if it was written
    bool Compact(std::string_view name);

    Filesystem& m_filesystem;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
};
