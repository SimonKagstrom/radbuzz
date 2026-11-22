#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "filesystem.hh"
#include "messages.hh"
#include "post_office.hh"

#include <array>
#include <string>
#include <string_view>

/**
 * @brief Records trips to the filesystem, one JSON line per GPS fix
 *
 * A trip is written to <root>/current/<UTC of the first fix, in hex>.jsl, where the root is
 * trips, or demo for trips started in demo mode. The lines are buffered, and appended when the
 * buffer is full or the vehicle stops. Only fixes with a GPS time, which move the pixel position
 * while the vehicle is moving, are recorded. On MSG::reset_trip, the trip is moved to
 * <root>/raw, MSG::new_recorded_trip is sent, and the next fix starts a new trip.
 *
 * At startup, a trip left in current (i.e., at power-off) is closed: a real one is moved to raw
 * like on a reset, and a demo one is removed, so that demo trips are only kept on purpose.
 */
class TripRecorder : public os::BaseThread
{
public:
    static constexpr auto kTripsRoot = "trips";
    static constexpr auto kDemoRoot = "demo";
    static constexpr auto kRoots = std::array {kTripsRoot, kDemoRoot};

    static constexpr auto kCurrentDirectory = "current";
    static constexpr auto kRawDirectory = "raw";
    static constexpr auto kBufferSize = 4096uz;

    TripRecorder(ApplicationState& state,
                 PostOffice<MSG::AllMessages>& post_office,
                 Filesystem& filesystem);

    /// A directory in a root, e.g., trips/raw
    static std::string Directory(std::string_view root, std::string_view directory);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void Record(const GpsData& data);
    void Flush();

    /// The path of the current trip in a directory of its root
    std::string PathIn(std::string_view directory) const;

    ApplicationState& m_state;
    Filesystem& m_filesystem;
    ListenerCookie m_listener;
    ApplicationState::PartialReadOnlyCache<AS::pixel_position, AS::is_moving> m_state_cache;
    PostOffice<MSG::AllMessages>& m_post_office;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;

    /// The root of the trip, set at its first fix
    std::string_view m_root {kTripsRoot};
    /// The filename of the trip, empty until its first fix
    std::string m_filename;
    std::string m_buffer;
    uint32_t m_last_utc {0};
};
