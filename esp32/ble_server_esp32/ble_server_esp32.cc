// From https://github.com/SIMS-IOT-Devices/FreeRTOS-ESP-IDF-BLE-Server/blob/main/proj3.c
#include "ble_server_esp32.hh"
//#include <esp_nimble_hci.h>

#include <algorithm>
#include <esp_err.h>
extern "C" {
#include <services/ans/ble_svc_ans.h>
}
namespace
{

constexpr auto kBluetoothBaseUuid =
    hal::detail::StringToUuid128("00000000-0000-1000-8000-00805f9b34fb");

constexpr hal::Uuid128
UuidFrom16(uint16_t value)
{
    auto uuid = kBluetoothBaseUuid;
    uuid[12] = static_cast<uint8_t>(value & 0xff);
    uuid[13] = static_cast<uint8_t>((value >> 8) & 0xff);
    return uuid;
}

constexpr hal::Uuid128
UuidFrom32(uint32_t value)
{
    auto uuid = kBluetoothBaseUuid;
    uuid[12] = static_cast<uint8_t>(value & 0xff);
    uuid[13] = static_cast<uint8_t>((value >> 8) & 0xff);
    uuid[14] = static_cast<uint8_t>((value >> 16) & 0xff);
    uuid[15] = static_cast<uint8_t>((value >> 24) & 0xff);
    return uuid;
}

constexpr hal::Uuid128
UuidFromBle(const ble_uuid_t& uuid)
{
    switch (uuid.type)
    {
    case BLE_UUID_TYPE_16:
        return UuidFrom16(reinterpret_cast<const ble_uuid16_t&>(uuid).value);
    case BLE_UUID_TYPE_32:
        return UuidFrom32(reinterpret_cast<const ble_uuid32_t&>(uuid).value);
    case BLE_UUID_TYPE_128: {
        hal::Uuid128 result {};
        std::copy(std::begin(reinterpret_cast<const ble_uuid128_t&>(uuid).value),
                  std::end(reinterpret_cast<const ble_uuid128_t&>(uuid).value),
                  result.begin());
        return result;
    }
    default:
        return {};
    }
}

constexpr bool
UuidEquals(const hal::Uuid128& lhs, const hal::Uuid128& rhs)
{
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

void
PrintUuid(const hal::Uuid128& uuid)
{
    printf("Service UUID: ");
    for (auto byte : uuid)
    {
        printf("%02x", byte);
    }
    printf("\n");
}

class Characteristic : public hal::IBleClient::ICharacteristic
{
public:
    Characteristic(BleServerEsp32& parent,
                   uint16_t conn_handle,
                   hal::Uuid128 uuid,
                   uint16_t value_handle,
                   uint16_t cccd_handle)
        : m_parent(parent)
        , m_conn_handle(conn_handle)
        , m_uuid(uuid)
        , m_value_handle(value_handle)
        , m_cccd_handle(cccd_handle)
    {
    }

    hal::Uuid128 GetUuid() const final
    {
        return m_uuid;
    }

    bool Write(std::span<const uint8_t> data) final
    {
        return m_parent.WritePeerCharacteristic(m_conn_handle, m_value_handle, data);
    }

    bool Read(DataCallback) final
    {
        return false;
    }

    bool Subscribe(DataCallback cb) final
    {
        if (m_cccd_handle == 0)
        {
            return false;
        }

        m_parent.RegisterNotificationCallback(m_value_handle, std::move(cb));
        return m_parent.EnablePeerNotifications(m_conn_handle, m_cccd_handle);
    }

    void Unsubscribe() final
    {
        m_parent.UnregisterNotificationCallback(m_value_handle);

        if (m_cccd_handle == 0 || m_conn_handle == BLE_HS_CONN_HANDLE_NONE)
        {
            return;
        }

        uint8_t notify_disable[2] = {0x00, 0x00};
        ble_gattc_write_flat(
            m_conn_handle,
            m_cccd_handle,
            notify_disable,
            sizeof(notify_disable),
            [](uint16_t, const struct ble_gatt_error*, struct ble_gatt_attr*, void*) { return 0; },
            nullptr);
    }

private:
    BleServerEsp32& m_parent;
    const uint16_t m_conn_handle;
    const hal::Uuid128 m_uuid;
    const uint16_t m_value_handle;
    const uint16_t m_cccd_handle;
};

class Service : public hal::IBleClient::IService
{
public:
    Service(hal::Uuid128 uuid, std::vector<std::unique_ptr<Characteristic>> characteristics)
        : m_uuid(uuid)
        , m_characteristics(std::move(characteristics))
    {
    }

    hal::Uuid128 GetUuid() const final
    {
        return m_uuid;
    }

    std::vector<hal::IBleClient::ICharacteristic*> GetCharacteristics() final
    {
        std::vector<hal::IBleClient::ICharacteristic*> characteristics;
        characteristics.reserve(m_characteristics.size());

        for (auto& characteristic : m_characteristics)
        {
            characteristics.push_back(characteristic.get());
        }

        return characteristics;
    }

private:
    const hal::Uuid128 m_uuid;
    std::vector<std::unique_ptr<Characteristic>> m_characteristics;
};


class Peer : public hal::IBleClient::IPeer
{
public:
    Peer(uint16_t conn_handle, BleServerEsp32& parent, std::unique_ptr<Service> service)
        : m_conn_handle(conn_handle)
        , m_parent(parent)
        , m_services()
    {
        m_services.push_back(std::move(service));
    }

    bool IsConnected() const final
    {
        return m_conn_handle != BLE_HS_CONN_HANDLE_NONE;
    }

    void Disconnect() final
    {
        if (m_conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ble_gap_terminate(m_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }

    std::vector<hal::IBleClient::IService*> GetServices() final
    {
        std::vector<hal::IBleClient::IService*> services;
        services.reserve(m_services.size());

        for (auto& service : m_services)
        {
            services.push_back(service.get());
        }

        return services;
    }

private:
    const uint16_t m_conn_handle;
    BleServerEsp32& m_parent;
    std::vector<std::unique_ptr<Service>> m_services;
};


BleServerEsp32* g_server;

} // namespace

BleServerEsp32::BleServerEsp32()
{
    g_server = this;
}


BleServerEsp32::~BleServerEsp32()
{
    g_server = nullptr;
}

bool
BleServerEsp32::WritePeerCharacteristic(uint16_t conn_handle,
                                        uint16_t value_handle,
                                        std::span<const uint8_t> data)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || value_handle == 0)
    {
        return false;
    }

    auto rc = ble_gattc_write_flat(
        conn_handle,
        value_handle,
        data.data(),
        data.size(),
        [](uint16_t conn_handle,
           const struct ble_gatt_error* error,
           struct ble_gatt_attr* attr,
           void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->PeerWriteComplete(conn_handle, error, attr);
        },
        this);

    return rc == 0;
}

void
BleServerEsp32::RegisterNotificationCallback(uint16_t value_handle,
                                             std::function<void(std::span<const uint8_t>)> cb)
{
    m_notification_callbacks[value_handle] = std::move(cb);
}

void
BleServerEsp32::UnregisterNotificationCallback(uint16_t value_handle)
{
    m_notification_callbacks.erase(value_handle);
}

bool
BleServerEsp32::EnablePeerNotifications(uint16_t conn_handle, uint16_t cccd_handle)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || cccd_handle == 0)
    {
        return false;
    }

    uint8_t notify_enable[2] = {0x01, 0x00};
    auto rc = ble_gattc_write_flat(
        conn_handle,
        cccd_handle,
        notify_enable,
        sizeof(notify_enable),
        [](uint16_t ch, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->PeerWriteComplete(ch, error, attr);
        },
        this);

    return rc == 0;
}

std::unique_ptr<ListenerCookie>
BleServerEsp32::AttachConnectionListener(std::function<void(bool connected)> cb)
{
    // NYI
    return std::make_unique<ListenerCookie>([this]() { /* Remove the listener */ });
}

void
BleServerEsp32::SetServiceUuid128(hal::Uuid128Span service_uuid)
{
    memcpy(m_service_uuid.value, service_uuid.data(), service_uuid.size());
}

void
BleServerEsp32::AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                            std::function<void(std::span<const uint8_t>)> cb)
{
    auto w = std::make_unique<WriteCharacteristic>();

    memcpy(w->uuid.value, uuid.data(), uuid.size());

    w->cb = cb;

    w->gatt_chr.arg = static_cast<void*>(w.get());
    w->gatt_chr.uuid = reinterpret_cast<const ble_uuid_t*>(&w->uuid);
    w->gatt_chr.flags = BLE_GATT_CHR_F_WRITE;
    w->gatt_chr.access_cb = [](uint16_t conn_handle,
                               uint16_t attr_handle,
                               struct ble_gatt_access_ctxt* ctxt,
                               void* arg) {
        auto p = reinterpret_cast<WriteCharacteristic*>(arg);

        auto data_size = OS_MBUF_PKTLEN(ctxt->om);
        auto flattened = std::make_unique<uint8_t[]>(data_size);
        uint16_t out_sz = 0;
        auto rv = ble_hs_mbuf_to_flat(ctxt->om, flattened.get(), data_size, &out_sz);

        if (rv == 0)
        {
            auto payload_span = std::span<const uint8_t> {
                reinterpret_cast<const uint8_t*>(flattened.get()), static_cast<size_t>(out_sz)};
            p->cb(payload_span);
        }
        ble_gatts_chr_updated(attr_handle);

        return 0;
    };

    m_characteristics.push_back(std::move(w));
}


void
BleServerEsp32::ScanForService(hal::Uuid128Span service_uuid,
                               const std::function<void(std::unique_ptr<IPeer>)>& cb)
{
    m_peer_service_uuid = hal::Uuid128 {};
    std::copy(service_uuid.begin(), service_uuid.end(), m_peer_service_uuid->begin());
    m_peer_found_cb = cb;

    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {};
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    /* Start discovery for 120 seconds */
    rc = ble_gap_disc(
        own_addr_type,
        (120 * 1000),
        &disc_params,
        [](struct ble_gap_event* event, void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->BleGapEvent(event);
        },
        this);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
}


void
BleServerEsp32::Start()
{
    // Setup the characteristics
    for (const auto& c : m_characteristics)
    {
        m_ble_gatt_chr_defs.push_back(c->gatt_chr);
    }
    // Terminator
    m_ble_gatt_chr_defs.push_back({0});

    m_gatt_svc_def.push_back({0});
    m_gatt_svc_def.back().type = BLE_GATT_SVC_TYPE_PRIMARY;
    m_gatt_svc_def.back().uuid = reinterpret_cast<const ble_uuid_t*>(&m_service_uuid);
    m_gatt_svc_def.back().includes = nullptr;
    m_gatt_svc_def.back().characteristics = m_ble_gatt_chr_defs.data();
    m_gatt_svc_def.push_back({0});

    nvs_flash_init(); // 1 - Initialize NVS flash using
    //esp_nimble_init();
    //esp_nimble_hci_init();                     // 2 - Initialize ESP controller
    nimble_port_init();  // 3 - Initialize the host stack
    ble_svc_gap_init();  // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init(); // 4 - Initialize NimBLE configuration - gatt service
    ble_svc_ans_init();
    ble_svc_gap_device_name_set("Bicycletas"); // 4 - Initialize NimBLE configuration - server name
    ble_gatts_count_cfg(
        m_gatt_svc_def.data()); // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(
        m_gatt_svc_def.data()); // 4 - Initialize NimBLE configuration - queues gatt services.

    ble_gap_write_sugg_def_data_len(0xfb, 0x1000);

    ble_hs_cfg.sync_cb = []() // 5 - Initialize application
    {
        ble_hs_id_infer_auto(
            0, &g_server->m_ble_addr_type); // Determines the best address type automatically
        g_server->AppAdvertise();           // Define the BLE connection
    };
}

void
BleServerEsp32::AppAdvertise()
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char* device_name;

    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t*)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable

    ble_gap_adv_start(
        m_ble_addr_type,
        NULL,
        BLE_HS_FOREVER,
        &adv_params,
        [](struct ble_gap_event* event, void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->BleGapEvent(event);
        },
        static_cast<void*>(this));
}

bool
BleServerEsp32::ConnectIfPeerMatches(const struct ble_gap_disc_desc* disc)
{
    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
        disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
    {
        return false;
    }

    struct ble_hs_adv_fields fields;
    auto rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0)
    {
        return false;
    }
    if (fields.name != nullptr && fields.name_len > 0)
    {
        printf("Found name: %.*s\n", fields.name_len, reinterpret_cast<const char*>(fields.name));
    }
    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           disc->addr.val[5],
           disc->addr.val[4],
           disc->addr.val[3],
           disc->addr.val[2],
           disc->addr.val[1],
           disc->addr.val[0]);
    // Match the service UUID
    printf(
        "VOBB: %d 16, %d 32 %d 128\n", fields.num_uuids16, fields.num_uuids32, fields.num_uuids128);
    for (auto i = 0; i < fields.num_uuids16; i++)
    {
        printf("Found service UUID: %04x\n", fields.uuids16[i].value);
        if (m_peer_service_uuid &&
            UuidEquals(*m_peer_service_uuid, UuidFromBle(fields.uuids16[i].u)))
        {
            return true;
        }
    }

    for (auto i = 0; i < fields.sol_num_uuids16; i++)
    {
        printf("Found service sol UUID: %04x\n", fields.sol_uuids16[i].value);
    }


    for (auto i = 0; i < fields.num_uuids128; i++)
    {
        auto uuid = UuidFromBle(fields.uuids128[i].u);
        if (m_peer_service_uuid && UuidEquals(*m_peer_service_uuid, uuid))
        {
            return true;
        }
        printf("Found service UUID: "
               "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
               fields.uuids128[i].value[15],
               fields.uuids128[i].value[14],
               fields.uuids128[i].value[13],
               fields.uuids128[i].value[12],
               fields.uuids128[i].value[11],
               fields.uuids128[i].value[10],
               fields.uuids128[i].value[9],
               fields.uuids128[i].value[8],
               fields.uuids128[i].value[7],
               fields.uuids128[i].value[6],
               fields.uuids128[i].value[5],
               fields.uuids128[i].value[4],
               fields.uuids128[i].value[3],
               fields.uuids128[i].value[2],
               fields.uuids128[i].value[1],
               fields.uuids128[i].value[0]);
    }

    for (auto i = 0; i < fields.num_uuids32; i++)
    {
        printf("Found service UUID: %08x\n", fields.uuids32[i].value);
        if (m_peer_service_uuid &&
            UuidEquals(*m_peer_service_uuid, UuidFromBle(fields.uuids32[i].u)))
        {
            return true;
        }
    }

    return false;
}

int
BleServerEsp32::PeerSvcDisced(uint16_t conn_handle,
                              const struct ble_gatt_error* error,
                              const struct ble_gatt_svc* service)
{
    int rc = -1;

    switch (error->status)
    {
    case 0:
        if (m_peer_service_uuid && UuidEquals(*m_peer_service_uuid, UuidFromBle(service->uuid.u)))
        {
            m_peer_svc_start_handle = service->start_handle;
            m_peer_svc_end_handle = service->end_handle;
        }
        rc = 0;
        break;

    case BLE_HS_EDONE:
        printf("Service discovery done\n");
        if (m_peer_svc_start_handle != 0 && m_peer_svc_end_handle != 0)
        {
            rc = StartPeerChrDiscovery(conn_handle);
        }
        else
        {
            rc = 0;
        }
        /* All descriptors in this characteristic discovered; start discovering
         * descriptors in the next characteristic.
         */
        //        if (peer->disc_prev_chr_val > 0) {
        //            peer_disc_dscs(peer);
        //        }
        break;

    default:
        /* Error; abort discovery. */
        rc = error->status;
        break;
    }

    if (rc != 0)
    {
        /* Error; abort discovery. */
        //        peer_disc_complete(peer, rc);
    }

    return rc;
}

int
BleServerEsp32::PeerChrDisced(uint16_t conn_handle,
                              const struct ble_gatt_error* error,
                              const struct ble_gatt_chr* chr)
{
    int rc = -1;

    switch (error->status)
    {
    case 0:
        if (m_peer_chr_val_handle == 0)
        {
            m_peer_char_uuid = UuidFromBle(chr->uuid.u);
            m_peer_chr_val_handle = chr->val_handle;
        }
        rc = 0;
        break;

    case BLE_HS_EDONE:
        printf("Characteristic discovery done\n");
        if (m_peer_chr_val_handle != 0)
        {
            rc = StartPeerDscDiscovery(conn_handle);
        }
        else
        {
            rc = 0;
        }
        break;

    default:
        rc = error->status;
        break;
    }

    return rc;
}

int
BleServerEsp32::PeerDscDisced(uint16_t conn_handle,
                              const struct ble_gatt_error* error,
                              uint16_t chr_val_handle,
                              const struct ble_gatt_dsc* dsc)
{
    int rc = -1;

    switch (error->status)
    {
    case 0: {
        auto dsc_uuid = ble_uuid_u16(&dsc->uuid.u);
        if (dsc_uuid == BLE_GATT_DSC_CLT_CFG_UUID16)
        {
            m_peer_cccd_handle = dsc->handle;
            rc = 0;
        }
        else
        {
            rc = 0;
        }
        break;
    }

    case BLE_HS_EDONE:
        printf("Descriptor discovery done\n");
        if (m_peer_service_uuid && m_peer_char_uuid && m_peer_chr_val_handle != 0)
        {
            std::vector<std::unique_ptr<Characteristic>> characteristics;
            characteristics.push_back(std::make_unique<Characteristic>(
                *this, conn_handle, *m_peer_char_uuid, m_peer_chr_val_handle, m_peer_cccd_handle));

            auto service =
                std::make_unique<Service>(*m_peer_service_uuid, std::move(characteristics));
            m_peer_found_cb(std::make_unique<Peer>(conn_handle, *this, std::move(service)));
        }
        rc = 0;
        break;

    default:
        rc = error->status;
        break;
    }

    return rc;
}

int
BleServerEsp32::PeerWriteComplete(uint16_t conn_handle,
                                  const struct ble_gatt_error* error,
                                  struct ble_gatt_attr* attr)
{
    if (error->status == 0)
    {
        printf("Write complete (handle=%u)\n", attr ? attr->handle : 0);
        return 0;
    }

    MODLOG_DFLT(ERROR, "Write failed; rc=%d\n", error->status);
    return error->status;
}

int
BleServerEsp32::StartPeerSvcDiscovery(uint16_t conn_handle)
{
    ble_uuid128_t svc_uuid {.u = {.type = BLE_UUID_TYPE_128}, .value = {}};
    std::copy(m_peer_service_uuid->begin(), m_peer_service_uuid->end(), svc_uuid.value);

    m_peer_svc_start_handle = 0;
    m_peer_svc_end_handle = 0;
    m_peer_char_uuid.reset();
    m_peer_chr_val_handle = 0;
    m_peer_cccd_handle = 0;

    return ble_gattc_disc_svc_by_uuid(
        conn_handle,
        &svc_uuid.u,
        [](uint16_t ch,
           const struct ble_gatt_error* error,
           const struct ble_gatt_svc* service,
           void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->PeerSvcDisced(ch, error, service);
        },
        this);
}

int
BleServerEsp32::StartPeerChrDiscovery(uint16_t conn_handle)
{
    if (m_peer_svc_start_handle == 0 || m_peer_svc_end_handle == 0)
    {
        return 0;
    }

    return ble_gattc_disc_all_chrs(
        conn_handle,
        m_peer_svc_start_handle,
        m_peer_svc_end_handle,
        [](uint16_t ch,
           const struct ble_gatt_error* error,
           const struct ble_gatt_chr* chr,
           void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->PeerChrDisced(ch, error, chr);
        },
        this);
}

int
BleServerEsp32::StartPeerDscDiscovery(uint16_t conn_handle)
{
    if (m_peer_chr_val_handle == 0 || m_peer_svc_end_handle == 0)
    {
        return 0;
    }

    uint16_t start_handle = m_peer_chr_val_handle + 1;
    if (start_handle > m_peer_svc_end_handle)
    {
        return 0;
    }

    return ble_gattc_disc_all_dscs(
        conn_handle,
        start_handle,
        m_peer_svc_end_handle,
        [](uint16_t ch,
           const struct ble_gatt_error* error,
           uint16_t chr_val_handle,
           const struct ble_gatt_dsc* dsc,
           void* arg) {
            auto p = reinterpret_cast<BleServerEsp32*>(arg);
            return p->PeerDscDisced(ch, error, chr_val_handle, dsc);
        },
        this);
}

int
BleServerEsp32::BleGapEvent(struct ble_gap_event* event)
{
    printf("T: %d\n", event->type);
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_DISC:
        if (m_peer_service_uuid && ConnectIfPeerMatches(&event->disc))
        {
            // Stop scanning
            auto rc = ble_gap_disc_cancel();
            if (rc != 0)
            {
                MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
                break;
            }

            /* Figure out address to use for connect (no privacy for now) */
            uint8_t own_addr_type;
            rc = ble_hs_id_infer_auto(0, &own_addr_type);
            if (rc != 0)
            {
                MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
                break;
            }

            // Try to connect the the advertiser.
            auto addr = &((struct ble_gap_disc_desc*)&event->disc)->addr;
            rc = ble_gap_connect(
                own_addr_type,
                addr,
                30000,
                NULL,
                [](struct ble_gap_event* event, void* arg) {
                    auto p = reinterpret_cast<BleServerEsp32*>(arg);
                    return p->BleGapEvent(event);
                },
                this);
            if (rc != 0)
            {
                MODLOG_DFLT(ERROR, "Failed to connect to device \n");
                break;
            }
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            //            AppAdvertise();
        }

        if (event->connect.status == 0)
        {
            m_peer_conn_handle = event->connect.conn_handle;
            m_notification_callbacks.clear();
            auto rc = StartPeerSvcDiscovery(event->connect.conn_handle);
            if (rc != 0)
            {
                MODLOG_DFLT(ERROR, "Failed to start peer service discovery; rc=%d\n", rc);
                return rc;
            }
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECT %d", event->disconnect.reason);
        m_peer_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        m_peer_service_uuid.reset();
        m_peer_svc_start_handle = 0;
        m_peer_svc_end_handle = 0;
        m_peer_char_uuid.reset();
        m_peer_chr_val_handle = 0;
        m_peer_cccd_handle = 0;
        m_notification_callbacks.clear();
        AppAdvertise();
        break;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        auto data_size = OS_MBUF_PKTLEN(event->notify_rx.om);
        auto flattened = std::make_unique<uint8_t[]>(data_size);
        uint16_t out_sz = 0;
        auto rv = ble_hs_mbuf_to_flat(event->notify_rx.om, flattened.get(), data_size, &out_sz);
        if (rv == 0)
        {
            auto handle = event->notify_rx.attr_handle;
            auto cb_it = m_notification_callbacks.find(handle);
            if (cb_it != m_notification_callbacks.end())
            {
                auto payload_span = std::span<const uint8_t> {
                    reinterpret_cast<const uint8_t*>(flattened.get()), static_cast<size_t>(out_sz)};
                cb_it->second(payload_span);
            }
            else
            {
                printf("Notify (%u bytes) on unhandled handle %u: ", out_sz, handle);
                for (uint16_t i = 0; i < out_sz; ++i)
                {
                    printf("%02x", flattened[i]);
                }
                printf("\n");
            }
        }
        break;
    }
    case BLE_GAP_EVENT_DATA_LEN_CHG:
        printf("LC: %d and %d\n",
               event->data_len_chg.max_rx_octets,
               event->data_len_chg.max_tx_octets);
        break;
    case BLE_GAP_EVENT_MTU:
        printf("MTU: %d\n", event->mtu.value);
        break;

    // Advertise again after completion of the event
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        AppAdvertise();
        break;
    default:
        break;
    }

    return 0;
}

void
BleServerEsp32::PollEvents()
{
    // From https://github.com/espressif/esp-idf/issues/3555#issuecomment-497148594
    auto eventq = nimble_port_get_dflt_eventq();

    struct ble_npl_event* ev;
    do
    {
        ev = ble_npl_eventq_get(eventq, 0);

        if (ev != nullptr)
        {
            ble_npl_event_run(ev);
        }
    } while (ev);
}
