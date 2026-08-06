#pragma once

#include "base_thread.hh"
#include "hal/i_ble_client.hh"
#include "king_shark_packet_protocol.hh"

class BleHandler;
class BleKingSharkHandler
{
public:
    BleKingSharkHandler(BleHandler& parent, hal::IBleClient& ble_client);

    void OnStartup();

    void Update();


private:
    enum class State
    {
        kStartup,
        kWaitForPeer,
        kConnected,

        kValueCount,
    };

    void OnBatteryData(std::span<const uint8_t> data);

    BleHandler& m_parent;
    hal::IBleClient& m_ble_client;

    KingSharkPacketProtocol m_packet_protocol;

    std::unique_ptr<hal::IBleClient::IPeer> m_battery_peer;
    hal::IBleClient::ICharacteristic* m_battery_cmd_char {nullptr};

    State m_state {State::kStartup};
    os::TimerHandle m_poll_timer;
    os::TimerHandle m_invalidate_timer;
};
