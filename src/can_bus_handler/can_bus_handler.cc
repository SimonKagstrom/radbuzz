#include "can_bus_handler.hh"

#include <vesc_buffer.h>
#include <vesc_can_sdk.h>

CanBusHandler::CanBusHandler(hal::ICan& bus, ApplicationState& app_state, uint8_t controller_id)
    : m_bus(bus)
    , m_state(app_state)
    , m_controller_id(controller_id)
{
    vesc_can_init(
        [](uint32_t id, const uint8_t* data, uint8_t len, void* user_cookie) {
            auto pThis = static_cast<CanBusHandler*>(user_cookie);
            return pThis->m_bus.SendFrame(id, std::span<const uint8_t> {data, len});
        },
        controller_id, // Receiver controller ID
        0x02,          // Sender ID
        this);

    vesc_set_response_callback([](uint8_t controller_id,
                                  uint8_t command,
                                  const uint8_t* data,
                                  uint8_t len,
                                  void* user_cookie) {
        auto pThis = static_cast<CanBusHandler*>(user_cookie);
        pThis->VescResponseCallback(controller_id, command, data, len);
    });
}

void
CanBusHandler::OnStartup()
{
    m_bus_listener = m_bus.Start(GetSemaphore());

    vesc_get_values_setup(m_controller_id);

    m_periodic_timer = StartTimer(500ms, [this]() {
        vesc_get_values_setup_selective(m_controller_id,
                                        SETUP_VALUE_SPEED | SETUP_VALUE_ODOMETER |
                                            SETUP_VALUE_INPUT_VOLTAGE_FILTERED);
        return 500ms;
    });
}

std::optional<milliseconds>
CanBusHandler::OnActivation()
{
    while (auto frame = m_bus.ReceiveFrame())
    {
        auto d = frame->Data();
        vesc_process_can_frame(frame->Id(), d.data(), static_cast<uint8_t>(d.size()));
    }

    return std::nullopt;
}

void
CanBusHandler::VescResponseCallback(uint8_t /*controller_id*/,
                                    uint8_t command,
                                    const uint8_t* data,
                                    uint8_t len)
{
    if (len < 1)
    {
        return;
    }

    auto ro = m_state.CheckoutReadonly();
    if (ro.Get<AS::demo_mode>())
    {
        // Don't update state in demo mode
        return;
    }

    auto qw = m_state.CheckoutQueuedWriter<
        AS::wh_consumed,
        AS::wh_regenerated,
        AS::distance_traveled,
        AS::current_power_w,
        AS::battery_millivolts, // Millivolts is temporary until the bms reader is done
        AS::controller_temperature,
        AS::motor_temperature,
        AS::speed,
        AS::max_speed>();

    if (command == CAN_PACKET_STATUS_3)
    {
        vesc_status_msg_3_t status;
        if (vesc_parse_status_msg_3(data, len, &status))
        {
            qw.Set<AS::wh_consumed>(status.watt_hours);
            qw.Set<AS::wh_regenerated>(status.watt_hours_charged);
        }
    }
    else if (command == CAN_PACKET_STATUS_4)
    {
        vesc_status_msg_4_t status;
        if (vesc_parse_status_msg_4(data, len, &status))
        {
            auto amps = status.current_in;

            auto watts = ro.Get<AS::battery_millivolts>() * amps / 1000.0f;

            qw.Set<AS::current_power_w>(static_cast<int16_t>(watts));
            qw.Set<AS::controller_temperature>(static_cast<uint8_t>(status.temp_fet));
            qw.Set<AS::motor_temperature>(static_cast<uint8_t>(status.temp_motor));
        }
    }
    else if (command == CAN_PACKET_STATUS_5)
    {
        vesc_status_msg_5_t status;
        if (vesc_parse_status_msg_5(data, len, &status))
        {
            // Store and cap to one decimal place
            qw.Set<AS::battery_millivolts>((static_cast<uint16_t>(status.v_in * 1000.0f) / 100) *
                                           100);
        }
    }
    else if (command == COMM_GET_VALUES_SETUP)
    {
        vesc_values_setup_t status;
        if (vesc_parse_get_values_setup(data, len, &status))
        {
            // For now nothing
        }
    }
    else if (command == COMM_GET_VALUES_SETUP_SELECTIVE &&
             data[0] == COMM_GET_VALUES_SETUP_SELECTIVE)
    {
        int32_t index = 1; // Skip packet ID
        etl::bitset<22, uint32_t> mask(vesc_buffer_get_uint32(data, &index));

        for (auto i = mask.find_first(true); i != mask.npos; i = mask.find_next(true, i + 1))
        {
            switch (1 << i)
            {
            case vesc_setup_value_index_t::SETUP_VALUE_SPEED: {
                auto meters_per_second = vesc_buffer_get_float32(data, 1e3f, &index);
                auto km_per_hour = meters_per_second * 3.6f;
                auto speed = static_cast<uint8_t>(km_per_hour);

                qw.Set<AS::speed>(speed);
                qw.Set<AS::max_speed>(std::max(ro.Get<AS::max_speed>(), speed));
            }
            break;
            case vesc_setup_value_index_t::SETUP_VALUE_INPUT_VOLTAGE_FILTERED: {
                auto mv = vesc_buffer_get_float16(data, 0.01f, &index);
                qw.Set<AS::battery_millivolts>(mv);
            }
            break;
            case vesc_setup_value_index_t::SETUP_VALUE_ODOMETER:
                qw.Set<AS::distance_traveled>(vesc_buffer_get_uint32(data, &index));
                break;

            default:
                break;
            }
        }
    }
}
