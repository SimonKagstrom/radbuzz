// From https://github.com/SIMS-IOT-Devices/FreeRTOS-ESP-IDF-BLE-Server/blob/main/proj3.c
#include "ble_server_esp32.hh"
//#include <esp_nimble_hci.h>

#include <esp_err.h>
extern "C" {
#include <services/ans/ble_svc_ans.h>
}
namespace
{

BleServerEsp32* g_server;
}

BleServerEsp32::BleServerEsp32()
{
    g_server = this;
}


BleServerEsp32::~BleServerEsp32()
{
    g_server = nullptr;
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
            p->cb(std::span<const uint8_t> {flattened.get(), out_sz});
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
    m_peer_service_uuid = hal::Uuid128();
    std::ranges::copy(service_uuid, m_peer_service_uuid->begin());

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

    // Match the service UUID
    printf("VOBB: %d 16, %d 128\n", fields.num_uuids16, fields.num_uuids128);
    for (auto i = 0; i < fields.num_uuids128; i++)
    {
        printf("Found service UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
               fields.uuids128[i].value[15], fields.uuids128[i].value[14], fields.uuids128[i].value[13],
               fields.uuids128[i].value[12], fields.uuids128[i].value[11], fields.uuids128[i].value[10],
               fields.uuids128[i].value[9], fields.uuids128[i].value[8], fields.uuids128[i].value[7],
               fields.uuids128[i].value[6], fields.uuids128[i].value[5], fields.uuids128[i].value[4],
               fields.uuids128[i].value[3], fields.uuids128[i].value[2], fields.uuids128[i].value[1],
               fields.uuids128[i].value[0]);
        if (fields.uuids128[i].value == m_peer_service_uuid->data())
        {
            return true;
        }
    }

    return false;
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
            AppAdvertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECT %d", event->disconnect.reason);
        AppAdvertise();
        break;
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
