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

    std::unique_ptr<ListenerCookie>
    AttachConnectionListener(std::function<void(bool connected)> cb) final;

    void SetServiceUuid128(hal::Uuid128Span service_uuid) final;

    void AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                     std::function<void(std::span<const uint8_t>)> data) final;


    void ScanForService(hal::Uuid16 service_uuid,
                        const std::function<void(std::unique_ptr<IPeer>)>& cb) final;

    void Start() final;

    void PollEvents() final;

    bool WriteToPeerFfe1(std::span<const uint8_t> data);

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
    int PeerSvcDisced(uint16_t conn_handle,
                      const struct ble_gatt_error* error,
                      const struct ble_gatt_svc* service);
    int PeerChrDisced(uint16_t conn_handle,
                      const struct ble_gatt_error* error,
                      const struct ble_gatt_chr* chr);
    int PeerDscDisced(uint16_t conn_handle,
                      const struct ble_gatt_error* error,
                      uint16_t chr_val_handle,
                      const struct ble_gatt_dsc* dsc);
    int PeerWriteComplete(uint16_t conn_handle,
                          const struct ble_gatt_error* error,
                          struct ble_gatt_attr* attr);
    int StartPeerSvcDiscovery(uint16_t conn_handle);
    int StartPeerChrDiscovery(uint16_t conn_handle);
    int StartPeerDscDiscovery(uint16_t conn_handle);
    int EnablePeerNotifications(uint16_t conn_handle);


    ble_uuid128_t m_service_uuid {.u = {.type = BLE_UUID_TYPE_128}, .value = {}};

    std::vector<std::unique_ptr<WriteCharacteristic>> m_characteristics;

    std::optional<hal::Uuid16> m_peer_service_uuid;
    std::function<void(std::unique_ptr<IPeer>)> m_peer_found_cb {[](auto x) {}};

    uint16_t m_peer_conn_handle {BLE_HS_CONN_HANDLE_NONE};
    uint16_t m_peer_svc_start_handle {0};
    uint16_t m_peer_svc_end_handle {0};
    uint16_t m_peer_chr_val_handle {0};
    uint16_t m_peer_cccd_handle {0};

    // Adaptations to the C interface
    uint8_t m_ble_addr_type {0};
    std::vector<struct ble_gatt_svc_def> m_gatt_svc_def;
    std::vector<struct ble_gatt_chr_def> m_ble_gatt_chr_defs;
};
