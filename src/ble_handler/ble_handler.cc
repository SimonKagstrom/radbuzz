#include "ble_handler.hh"

constexpr auto kServiceUuid = "ec91d7ab-e87c-48d5-adfa-cc4b2951298a";

constexpr auto kChaSettings = "9d37a346-63d3-4df6-8eee-f0242949f59f";
constexpr auto kChaNav = "0b11deef-1563-447f-aece-d3dfeb1c1f20";
constexpr auto kChaNavTbtIcon = "d4d8fcca-16b2-4b8e-8ed5-90137c44a8ad";
constexpr auto kChaNavTbtIconDesc = "d63a466e-5271-4a5d-a942-a34ccdb013d9";
constexpr auto kChaGpsSpeed = "98b6073a-5cf3-4e73-b6d3-f8e05fa018a9";

BleHandler::BleHandler(hal::IBleServer& server)
    : m_server(server)
{
}

void
BleHandler::OnStartup()
{
    m_ble_poller = StartTimer(20ms, [this]() {
        m_server.PollEvents();

        return 20ms;
    });

    m_server.SetServiceUuid128(hal::detail::StringToUuid128(kServiceUuid));

    m_server.AddWriteGattCharacteristics(hal::detail::StringToUuid128(kChaNav),
                                         [this](auto data) { OnChaNav(data); });

    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaSettings),
        [this](auto data) { printf(": %.*s\n", data.size(), (const char*)data.data()); });
    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaNavTbtIcon),
        [this](auto data) { printf(": %.*s\n", data.size(), (const char*)data.data()); });
    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaNavTbtIconDesc),
        [this](auto data) { printf(": %.*s\n", data.size(), (const char*)data.data()); });
    m_server.AddWriteGattCharacteristics(
        hal::detail::StringToUuid128(kChaGpsSpeed),
        [this](auto data) { printf(": %.*s\n", data.size(), (const char*)data.data()); });

    m_server.Start();
}

std::optional<milliseconds>
BleHandler::OnActivation()
{
    return std::nullopt;
}

void
BleHandler::OnChaNav(std::span<const uint8_t> data)
{
    printf("ChaNav: %.*s\n", data.size(), (const char*)data.data());
}
