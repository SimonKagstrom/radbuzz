#include "ble_king_shark_handler.hh"

#include "ble_handler.hh"

BleKingSharkHandler::BleKingSharkHandler(BleHandler& parent, hal::IBleClient& ble_client)
    : m_parent(parent)
    , m_ble_client(ble_client)
{
    auto ps = m_parent.m_state.CheckoutPartialSnapshot<AS::bms_data>();

    ps.GetWritableReference<AS::bms_data>().valid = false;
}

void
BleKingSharkHandler::OnStartup()
{
    m_ble_client.ScanForService(
        hal::detail::StringToUuid128("0000ffe0-0000-1000-8000-00805f9b34fb"), [this](auto peer) {
            m_battery_peer = std::move(peer);
            if (!m_battery_peer)
            {
                return;
            }

            auto services = m_battery_peer->GetServices();
            if (services.empty())
            {
                printf("Connected peer has no services\n");
                return;
            }

            auto characteristics = services.front()->GetCharacteristics();
            if (characteristics.empty())
            {
                printf("Connected service has no characteristics\n");
                return;
            }

            constexpr auto kFfe1Uuid =
                hal::detail::StringToUuid128("0000ffe1-0000-1000-8000-00805f9b34fb");
            for (auto* characteristic : characteristics)
            {
                auto uuid = characteristic->GetUuid();

                if (uuid == kFfe1Uuid)
                {
                    m_battery_cmd_char = characteristic;
                    break;
                }
            }
            if (m_battery_cmd_char == nullptr)
            {
                printf("FFE1 characteristic not found on connected peer\n");
                return;
            }
            auto subscribe_ok = m_battery_cmd_char->Subscribe(
                [this](std::span<const uint8_t> data) { OnBatteryData(data); });
            if (!subscribe_ok)
            {
                printf("Subscribe on FFE1 failed\n");
            }
        });
}

void
BleKingSharkHandler::Update()
{
    if (m_battery_cmd_char && m_poll_timer == nullptr)
    {
        m_poll_timer = m_parent.StartTimer(100ms, [this]() {
            auto command = m_packet_protocol.BuildTxPacket(0x16, std::array<uint8_t, 1> {0x00});

            if (command)
            {
                m_battery_cmd_char->Write(*command);
            }

            return 5s;
        });
    }
    {
    }
}

void
BleKingSharkHandler::OnBatteryData(std::span<const uint8_t> data)
{
    m_packet_protocol.PushData(data);
    if (auto packet = m_packet_protocol.Poll(); packet)
    {
        printf("King Shark packet: ");
        for (auto b : *packet)
        {
            printf("%02x", b);
        }
        printf("\n");

        auto& p = *packet;

        /*
         * This is the reply to an information packet. See king_shark_bms_protocol.md for
         * more info about the protocol.
         *
         * TODO: Fix this ugly hack
         */
        // Reply to an information packet (0x16). TODO: Fix this ugly crudeness
        if (p.size() > 16 && p[2] == 0x02 && p[3] == 0x61)
        {
            auto ps = m_parent.m_state.CheckoutPartialSnapshot<AS::bms_data, AS::battery_soc>();
            auto& bms_data = ps.GetWritableReference<AS::bms_data>();

            bms_data.valid = true;
            bms_data.soc = p[4];
            bms_data.highest_cell_temp = p[6];
            bms_data.bms_temperature = p[8]; // Other temperature

            // We now control the SoC
            ps.Set<AS::battery_soc>(bms_data.soc);

            // Invalidate if there are no more replies from the BMS
            m_invalidate_timer = m_parent.StartTimer(10s, [this]() {
                m_parent.m_state.CheckoutPartialSnapshot<AS::bms_data>()
                    .GetWritableReference<AS::bms_data>()
                    .valid = false;

                printf("BMS data invalidated due to timeout\n");
                return std::nullopt;
            });
        }
    }
}
