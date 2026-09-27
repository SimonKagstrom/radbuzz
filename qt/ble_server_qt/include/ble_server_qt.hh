#pragma once

#include "hal/i_ble_server.hh"

#include <QByteArray>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyServiceData>
#include <QObject>
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

/// A BLE peripheral using Qt Bluetooth.
///
/// The Qt objects live in the GUI thread, while the IBleServer interface is used from the
/// BLE handler thread. Incoming data is therefore queued, and delivered in PollEvents().
class BleServerQt : public QObject, public hal::IBleServer
{
public:
    explicit BleServerQt(QString device_name);

    std::unique_ptr<ListenerCookie>
    AttachConnectionListener(std::function<void(bool connected)> cb) final;

    void SetServiceUuid128(hal::Uuid128Span service_uuid) final;

    void AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                     std::function<void(std::span<const uint8_t>)> data_cb) final;

    void AddNotifyGattCharacteristics(hal::Uuid128Span uuid) final;

    bool Notify(hal::Uuid128Span uuid, std::span<const uint8_t> data) final;

    void Start() final;
    void PollEvents() final;

private:
    struct WriteCharacteristic
    {
        QBluetoothUuid uuid;
        std::function<void(std::span<const uint8_t>)> cb;
    };

    struct Event
    {
        // Set for connection changes
        std::optional<bool> connected;
        QBluetoothUuid uuid;
        QByteArray data;
    };

    // Called in the GUI thread
    void RequestPermissionAndSetup();
    void SetupPeripheral();
    void StartAdvertising();
    void SendNotification(const QBluetoothUuid& uuid, const QByteArray& data);
    void PushEvent(Event event);

    const QString m_device_name;

    QLowEnergyServiceData m_service_data;
    std::vector<WriteCharacteristic> m_write_characteristics;

    QLowEnergyController* m_controller {nullptr};
    QLowEnergyService* m_service {nullptr};

    std::atomic_bool m_connected {false};
    std::mutex m_event_mutex;
    std::deque<Event> m_events;

    std::function<void(bool connected)> m_on_connection_changed {[](bool) {}};
};
