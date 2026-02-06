#pragma once

#include "hal/i_ble_client.hh"
#include "hal/i_ble_server.hh"

#include <esp_log.h>
#include <esp_nimble_cfg.h>
#include <host/ble_hs.h>
#include <memory>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <vector>

class BleServerEsp32 : public hal::IBleServer, public hal::IBleClient
{
public:
    BleServerEsp32();
    ~BleServerEsp32() final;

    void SetServiceUuid128(hal::Uuid128Span service_uuid) final;

    void AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;


    void ScanForService(hal::Uuid128Span service_uuid,
                        const std::function<void(std::unique_ptr<IPeer>)>& cb) final;

    void Start() final;

    void PollEvents() final;

private:
    struct WriteCharacteristic
    {
        ble_uuid128_t uuid {.u = {.type = BLE_UUID_TYPE_128}, .value = {}};
        struct ble_gatt_chr_def gatt_chr {};
        std::function<void(std::span<const uint8_t>)> cb;
    };

    void AppAdvertise();
    int BleGapEvent(struct ble_gap_event* event);
    bool ConnectIfPeerMatches(const struct ble_gap_disc_desc* disc);


    ble_uuid128_t m_service_uuid {.u = {.type = BLE_UUID_TYPE_128}, .value = {}};

    std::vector<std::unique_ptr<WriteCharacteristic>> m_characteristics;

    std::optional<hal::Uuid128> m_peer_service_uuid;

    // Adaptations to the C interface
    uint8_t m_ble_addr_type {0};
    std::vector<struct ble_gatt_svc_def> m_gatt_svc_def;
    std::vector<struct ble_gatt_chr_def> m_ble_gatt_chr_defs;
};
