#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"

#include <string>

/**
 * @brief Records trips to the filesystem, one JSON line per GPS fix
 *
 * A trip is written to TRIPS/CURRENT/<UTC of the first fix, in hex>.JSL. The lines are
 * buffered, and appended when the buffer is full or the vehicle stops. Only fixes with a GPS
 * time, which move the pixel position while the vehicle is moving, are recorded, and nothing in
 * demo mode.
 */
class TripRecorder : public os::BaseThread
{
public:
    static constexpr auto kCurrentDirectory = "TRIPS/CURRENT";
    static constexpr auto kBufferSize = 4096uz;

    TripRecorder(ApplicationState& state, Filesystem& filesystem);

private:
    std::optional<milliseconds> OnActivation() final;

    void Record(const GpsData& data);
    void Flush();

    ApplicationState& m_state;
    Filesystem& m_filesystem;
    ListenerCookie m_listener;
    ApplicationState::PartialReadOnlyCache<AS::pixel_position, AS::is_moving> m_state_cache;

    /// Empty until the first fix of the trip
    std::string m_path;
    std::string m_buffer;
    uint32_t m_last_utc {0};
};
