#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "messages.hh"
#include "post_office.hh"

#include <string>

/**
 * @brief Records trips to the filesystem, one JSON line per GPS fix
 *
 * A trip is written to TRIPS/CURRENT/<UTC of the first fix, in hex>.JSL. The lines are
 * buffered, and appended when the buffer is full or the vehicle stops. Only fixes with a GPS
 * time, which move the pixel position while the vehicle is moving, are recorded, and nothing in
 * demo mode. On MSG::reset_trip, the trip is moved to TRIPS/RAW, and the next fix starts a new
 * one.
 */
class TripRecorder : public os::BaseThread
{
public:
    static constexpr auto kCurrentDirectory = "TRIPS/CURRENT";
    static constexpr auto kRawDirectory = "TRIPS/RAW";
    static constexpr auto kBufferSize = 4096uz;

    TripRecorder(ApplicationState& state,
                 PostOffice<MSG::AllMessages>& post_office,
                 Filesystem& filesystem);

private:
    std::optional<milliseconds> OnActivation() final;

    void Record(const GpsData& data);
    void Flush();

    /// The path of the current trip in a directory
    std::string PathIn(std::string_view directory) const;

    ApplicationState& m_state;
    Filesystem& m_filesystem;
    ListenerCookie m_listener;
    ApplicationState::PartialReadOnlyCache<AS::pixel_position, AS::is_moving> m_state_cache;
    PostOffice<MSG::AllMessages>& m_post_office;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;

    /// The filename of the trip, empty until its first fix
    std::string m_filename;
    std::string m_buffer;
    uint32_t m_last_utc {0};
};
