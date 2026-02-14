#include "can_bus_handler.hh"

#include <vesc_buffer.h>
#include <vesc_can_sdk.h>

CanBusHandler::CanBusHandler(hal::ICan& bus, ApplicationState& app_state, uint8_t controller_id)
    : m_bus(bus)
    , m_state(app_state)
    , m_controller_id(controller_id)
{
    m_state_listener = m_state.AttachListener(GetSemaphore());

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
                                        SETUP_VALUE_ODOMETER | SETUP_VALUE_INPUT_VOLTAGE_FILTERED);
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
CanBusHandler::VescResponseCallback(uint8_t controller_id,
                                    uint8_t command,
                                    const uint8_t* data,
                                    uint8_t len)
{
    if (len < 1)
    {
        return;
    }

    auto state = m_state.CheckoutReadWrite();

    if (command == CAN_PACKET_STATUS)
    {
        vesc_status_msg_1_t status;
        if (vesc_parse_status_msg_1(data, len, &status))
        {
            // TODO: Calculate based on RPM and wheel diameter + gear ratio
            state.Set<AS::speed>(static_cast<uint8_t>(status.rpm));
        }
    }
    else if (command == CAN_PACKET_STATUS_3)
    {
        vesc_status_msg_3_t status;
        if (vesc_parse_status_msg_3(data, len, &status))
        {
            //            printf("VESC#%d Status 3: Wh: %.2f, Wh charged: %.2f\n",
            //                   status.controller_id,
            //                   status.watt_hours,
            //                   status.watt_hours_charged);
        }
    }
    else if (command == CAN_PACKET_STATUS_4)
    {
        vesc_status_msg_4_t status;
        if (vesc_parse_status_msg_4(data, len, &status))
        {
            state.Set<AS::controller_temperature>(static_cast<uint8_t>(status.temp_fet));
            state.Set<AS::motor_temperature>(static_cast<uint8_t>(status.temp_motor));
        }
    }
    else if (command == CAN_PACKET_STATUS_5)
    {
        vesc_status_msg_5_t status;
        if (vesc_parse_status_msg_5(data, len, &status))
        {
            // Store and cap to one decimal place
            state.Set<AS::battery_millivolts>((static_cast<uint16_t>(status.v_in * 1000.0f) / 100) * 100);
        }
    }
    else if (command == COMM_GET_VALUES_SETUP)
    {
        vesc_values_setup_t status;
        if (vesc_parse_get_values_setup(data, len, &status))
        {
            printf("VESC#%d Setup Values: Speed: %.2f km/h. Odometer: %d. uptime: %d\n",
                   controller_id,
                   status.speed,
                   status.odometer,
                   status.system_time_ms);
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
            case vesc_setup_value_index_t::SETUP_VALUE_TEMP_FET_FILTERED:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_TEMP_MOTOR_FILTERED:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_CURRENT_TOT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_CURRENT_IN_TOT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_DUTY_CYCLE_NOW:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_RPM:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_SPEED:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_INPUT_VOLTAGE_FILTERED:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_BATTERY_LEVEL:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_AH_TOT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_AH_CHARGE_TOT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_WH_TOT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_WH_CHARGE_TOT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_DISTANCE:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_DISTANCE_ABS:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_PID_POS_NOW:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_FAULT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_SECOND_MOTOR_CONTROLLER_ID:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_NUM_VESCS:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_WH_BATT_LEFT:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_ODOMETER:
                break;
            case vesc_setup_value_index_t::SETUP_VALUE_SYSTEM_TIME_MS:
                break;
            }
        }
        auto input_voltage_filtered = vesc_buffer_get_float16(data, 1e3f, &index);
        auto odometer = vesc_buffer_get_uint32(data, &index);

        printf("Selective VESC#%d. %u bytes. Input Voltage Filtered: %.2f, Odometer: %u\n",
               controller_id,
               len,
               input_voltage_filtered,
               odometer);
    }
}
