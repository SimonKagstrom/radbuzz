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
 * On MSG::new_recorded_trip, every trip in <root>/RAW (for each TripRecorder root, i.e., TRIPS
 * and DEMO) is simplified to at most kMaxEntries points with the TripLog, and the kept lines are
 * copied unchanged to <root>/LOG, with the same name, and MSG::new_compacted_trip is sent. The
 * raw trip is then removed. Lines which can't be parsed are skipped. A trip which can't be
 * written is left in RAW, and is retried on the next message. RAW is also walked at startup.
 */
class TripCompactor : public PooledThreadBase
{
public:
    /// In the same root as the raw trip, see TripRecorder
    static constexpr auto kLogDirectory = "LOG";
    static constexpr auto kMaxEntries = 1000uz;

    TripCompactor(PostOffice<MSG::AllMessages>& post_office, Filesystem& filesystem);

private:
    enum class CompactResult
    {
        kWritten,
        kNoPoints,
        kFailed,
    };

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    /// Compact and remove every trip in RAW, in every root
    void WalkRawDirectory();

    /// Compact a trip from RAW to LOG in a root
    CompactResult Compact(std::string_view root, std::string_view name);

    PostOffice<MSG::AllMessages>& m_post_office;
    Filesystem& m_filesystem;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
};
