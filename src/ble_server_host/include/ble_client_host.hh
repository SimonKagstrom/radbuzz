#pragma once

#include "hal/i_ble_client.hh"

class BleClientHost : public hal::IBleClient
{
public:
    void
    ScanForService(hal::Uuid128Span service_uuid,
                   const std::function<void(std::unique_ptr<hal::IBleClient::IPeer>)>& cb) final
    {
        // Not relevant for now
    }
};
