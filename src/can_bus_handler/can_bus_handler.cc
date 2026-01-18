#include "can_bus_handler.hh"

#include <vesc_can_sdk.h>

CanBusHandler::CanBusHandler(hal::ICan& bus, ApplicationState& app_state, uint8_t controller_id)
    : m_bus(bus)
    , m_state(app_state)
    , m_controller_id(controller_id)
{
    m_state_listener = m_state.AttachListener(GetSemaphore());

    vesc_can_init(
        [](uint32_t id, const uint8_t* data, uint8_t len, void* user_cookie) {
            printf("Sending frame of %u\n", len);
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
}

std::optional<milliseconds>
CanBusHandler::OnActivation()
{
    while (auto frame = m_bus.ReceiveFrame())
    {
        vesc_process_can_frame(
            frame->id, frame->data.data(), static_cast<uint8_t>(frame->data.size()));
    }

    return std::nullopt;
}

void
CanBusHandler::VescResponseCallback(uint8_t controller_id,
                                    uint8_t command,
                                    const uint8_t* data,
                                    uint8_t len)
{
    if (command == CAN_PACKET_STATUS)
    {
        vesc_status_msg_1_t status;
        if (vesc_parse_status_msg_1(data, len, &status))
        {
            // TODO: Calculate based on RPM and wheel diameter + gear ratio
            m_state.Checkout()->speed = static_cast<uint8_t>(status.rpm);
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
            auto s = m_state.Checkout();

            s->controller_temperature = static_cast<uint8_t>(status.temp_fet);
            s->motor_temperature = static_cast<uint8_t>(status.temp_motor);
        }
    }
    else if (command == CAN_PACKET_STATUS_5)
    {
        vesc_status_msg_5_t status;
        if (vesc_parse_status_msg_5(data, len, &status))
        {
            // Store and cap to one decimal place
            m_state.Checkout()->battery_millivolts =
                (static_cast<uint16_t>(status.v_in * 1000.0f) / 100) * 100;
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
}
