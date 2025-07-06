#pragma once

#include "hal/i_ble_server.hh"

#include <esp_log.h>
#include <esp_nimble_cfg.h>
#include <esp_nimble_hci.h>
#include <host/ble_hs.h>
#include <memory>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <vector>

class BleServerEsp32 : public hal::IBleServer
{
public:
    BleServerEsp32();
    ~BleServerEsp32();

    void SetServiceUuid128(std::span<const uint8_t, 16> service_uuid) final;

    void AddWriteGattCharacteristics(std::span<const uint8_t, 16> uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;

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

    ble_uuid128_t m_service_uuid {.u = {.type = BLE_UUID_TYPE_128}, .value = {}};

    std::vector<std::unique_ptr<WriteCharacteristic>> m_characteristics;

    // Adaptations to the C interface
    uint8_t m_ble_addr_type {0};
    struct ble_gatt_svc_def m_gatt_svc_def;
    std::vector<struct ble_gatt_chr_def> m_ble_gatt_chr_defs;
};
