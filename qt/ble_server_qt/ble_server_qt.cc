#include "ble_server_qt.hh"

#include <QCoreApplication>
#include <QLowEnergyAdvertisingData>
#include <QLowEnergyAdvertisingParameters>
#include <QLowEnergyCharacteristicData>
#include <QLowEnergyDescriptorData>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif
#include <QUuid>

namespace
{

// hal::Uuid128 is little-endian (as in NimBLE)
QBluetoothUuid
ToQtUuid(hal::Uuid128Span uuid)
{
    const uint8_t* d = reinterpret_cast<const uint8_t*>(uuid.data());
    if constexpr (QSysInfo::ByteOrder == QSysInfo::LittleEndian)
    {
        uint32_t l = (d[3] << 24) | (d[2] << 16) | (d[1] << 8) | d[0];
        uint16_t w1 = (d[5] << 8) | d[4];
        uint16_t w2 = (d[7] << 8) | d[6];
        return QBluetoothUuid(
            QUuid(l, w1, w2, d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]));
    }
    else
    {
        uint32_t l = (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
        uint16_t w1 = (d[4] << 8) | d[5];
        uint16_t w2 = (d[6] << 8) | d[7];
        return QBluetoothUuid(
            QUuid(l, w1, w2, d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]));
    }
}

} // namespace

BleServerQt::BleServerQt(QString device_name)
    : m_device_name(std::move(device_name))
{
    m_service_data.setType(QLowEnergyServiceData::ServiceTypePrimary);
}

std::unique_ptr<ListenerCookie>
BleServerQt::AttachConnectionListener(std::function<void(bool connected)> cb)
{
    m_on_connection_changed = std::move(cb);

    return std::make_unique<ListenerCookie>([this]() { m_on_connection_changed = [](bool) {}; });
}

void
BleServerQt::SetServiceUuid128(hal::Uuid128Span service_uuid)
{
    m_service_data.setUuid(ToQtUuid(service_uuid));
}

void
BleServerQt::AddWriteGattCharacteristics(hal::Uuid128Span uuid,
                                         std::function<void(std::span<const uint8_t>)> data_cb)
{
    QLowEnergyCharacteristicData data;

    data.setUuid(ToQtUuid(uuid));
    data.setProperties(QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::WriteNoResponse);
    data.setValueLength(0, 512);
    m_service_data.addCharacteristic(data);

    m_write_characteristics.push_back({ToQtUuid(uuid), std::move(data_cb)});
}

void
BleServerQt::AddNotifyGattCharacteristics(hal::Uuid128Span uuid)
{
    QLowEnergyCharacteristicData data;

    data.setUuid(ToQtUuid(uuid));
    data.setProperties(QLowEnergyCharacteristic::Notify);
    data.setValueLength(0, 512);
#ifndef Q_OS_DARWIN
    // CoreBluetooth adds this by itself
    data.addDescriptor(QLowEnergyDescriptorData(
        QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration, QByteArray(2, 0)));
#endif
    m_service_data.addCharacteristic(data);
}

bool
BleServerQt::Notify(hal::Uuid128Span uuid, std::span<const uint8_t> data)
{
    if (!m_connected)
    {
        return false;
    }

    auto bytes = QByteArray(reinterpret_cast<const char*>(data.data()), data.size());
    QMetaObject::invokeMethod(
        this,
        [this, qt_uuid = ToQtUuid(uuid), bytes]() { SendNotification(qt_uuid, bytes); },
        Qt::QueuedConnection);

    return true;
}

void
BleServerQt::Start()
{
    // Called from the BLE handler thread, but Qt Bluetooth must be setup in the GUI thread
    QMetaObject::invokeMethod(
        this, [this]() { RequestPermissionAndSetup(); }, Qt::QueuedConnection);
}

void
BleServerQt::PollEvents()
{
    std::deque<Event> events;
    {
        std::scoped_lock lock(m_event_mutex);
        events.swap(m_events);
    }

    for (const auto& event : events)
    {
        if (event.connected)
        {
            m_on_connection_changed(*event.connected);
            continue;
        }

        for (const auto& characteristic : m_write_characteristics)
        {
            if (characteristic.uuid == event.uuid)
            {
                characteristic.cb({reinterpret_cast<const uint8_t*>(event.data.constData()),
                                   static_cast<size_t>(event.data.size())});
            }
        }
    }
}

void
BleServerQt::RequestPermissionAndSetup()
{
// Needed for MacOS, but let e.g., Ubuntu 24.04 still be able to compile the code
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QBluetoothPermission permission;
    permission.setCommunicationModes(QBluetoothPermission::Access |
                                     QBluetoothPermission::Advertise);

    switch (qApp->checkPermission(permission))
    {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(
            permission, this, [this](const QPermission&) { RequestPermissionAndSetup(); });
        break;
    case Qt::PermissionStatus::Denied:
        qWarning("BLE: Bluetooth permission denied");
        break;
    case Qt::PermissionStatus::Granted:
        SetupPeripheral();
        break;
    }
#endif
}

void
BleServerQt::SetupPeripheral()
{
    m_controller = QLowEnergyController::createPeripheral(this);
    m_service = m_controller->addService(m_service_data, this);
    if (m_service == nullptr)
    {
        qWarning("BLE: Failed to add service");
        return;
    }

    connect(m_service,
            &QLowEnergyService::characteristicChanged,
            this,
            [this](const QLowEnergyCharacteristic& characteristic, const QByteArray& value) {
                // Our own notifications also end up here
                if (characteristic.properties() & QLowEnergyCharacteristic::Notify)
                {
                    return;
                }
                PushEvent({std::nullopt, characteristic.uuid(), value});
            });

    connect(m_controller, &QLowEnergyController::connected, this, [this]() {
        qInfo("BLE: Connected to %s", qPrintable(m_controller->remoteAddress().toString()));
        m_connected = true;
        PushEvent({true, {}, {}});
    });
    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        qInfo("BLE: Disconnected");
        m_connected = false;
        PushEvent({false, {}, {}});
        StartAdvertising();
    });
    connect(m_controller,
            &QLowEnergyController::errorOccurred,
            this,
            [this](QLowEnergyController::Error error) {
                qWarning("BLE: Controller error %d: %s",
                         static_cast<int>(error),
                         qPrintable(m_controller->errorString()));
            });

    StartAdvertising();
}

void
BleServerQt::StartAdvertising()
{
    // A 128-bit UUID and the name don't both fit in the advertising data. Gadgetbridge
    // identifies the device by name, so that's the important part.
    QLowEnergyAdvertisingData advertising;
    advertising.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    advertising.setIncludePowerLevel(false);
    advertising.setLocalName(m_device_name);

    QLowEnergyAdvertisingData scan_response;
#ifndef Q_OS_DARWIN
    // CoreBluetooth ignores the scan response and puts everything in the advertising data.
    // It then drops the local name in favour of the UUID, and the computer name is shown.
    scan_response.setServices({m_service_data.uuid()});
#endif

    m_controller->startAdvertising(QLowEnergyAdvertisingParameters(), advertising, scan_response);
    qInfo("BLE: Advertising as '%s', service %s",
          qPrintable(m_device_name),
          qPrintable(m_service_data.uuid().toString()));
}

void
BleServerQt::SendNotification(const QBluetoothUuid& uuid, const QByteArray& data)
{
    if (m_service == nullptr)
    {
        return;
    }

    auto characteristic = m_service->characteristic(uuid);
    if (!characteristic.isValid())
    {
        qWarning("BLE: Notify on unknown characteristic %s", qPrintable(uuid.toString()));
        return;
    }

    // ATT notifications carry MTU - 3 bytes of payload
    const auto chunk_size = std::max(m_controller->mtu() - 3, 20);
    for (qsizetype offset = 0; offset < data.size(); offset += chunk_size)
    {
        m_service->writeCharacteristic(characteristic, data.mid(offset, chunk_size));
    }
}

void
BleServerQt::PushEvent(Event event)
{
    std::scoped_lock lock(m_event_mutex);

    m_events.push_back(std::move(event));
}
