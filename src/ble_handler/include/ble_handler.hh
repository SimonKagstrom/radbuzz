#pragma once

#include "hal/i_ble_server.hh"
#include "base_thread.hh"

class BleHandler : public os::BaseThread
{
public:
    BleHandler(hal::IBleServer &server);

private:
// From BaseThread
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void OnChaNav(std::span<const uint8_t> data);

    os::TimerHandle m_ble_poller;
    hal::IBleServer &m_server;
};
