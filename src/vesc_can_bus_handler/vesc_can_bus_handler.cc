#include "vesc_can_bus_handler.hh"

#include <numbers>
#include <vesc_buffer.h>
#include <vesc_can_sdk.h>

// Round up slightly above the limit
constexpr auto kProfileSpeedTable = std::array<uint8_t, static_cast<size_t>(Profile::kValueCount)> {
    5,
    27,
    33,
    47,
    200,
};

struct VescCanState
{
    std::optional<vesc_mcconf_t> mcconf;
};

VescCanBusHandler::VescCanBusHandler(hal::ICan& bus, ApplicationState& app_state)
    : m_bus(bus)
    , m_state(app_state)
    , m_state_cache(app_state)
    , m_state_listener(m_state.AttachListener<AS::configuration>(GetSemaphore()))
{
    m_vesc_can_state = new VescCanState();
}

VescCanBusHandler::~VescCanBusHandler()
{
    delete m_vesc_can_state;
}

void
VescCanBusHandler::OnStartup()
{
    m_bus_listener = m_bus.Start(GetSemaphore());

    auto ro = m_state.CheckoutReadonly();

    // storage.cc has loaded these before the thread start, so set here
    m_start_consumed_wh = ro.Get<AS::wh_consumed>();
    m_start_regen_wh = ro.Get<AS::wh_regenerated>();

    // Update so that conf changes can be read later
    m_state_cache.Pull();
}

std::optional<milliseconds>
VescCanBusHandler::OnActivation()
{

    while (auto frame = m_bus.ReceiveFrame())
    {
        auto d = frame->Data();

        if (!m_controller_id)
        {
            m_controller_id = frame->Id();

            vesc_can_init(
                [](uint32_t id, const uint8_t* data, uint8_t len, void* user_cookie) {
                    auto pThis = static_cast<VescCanBusHandler*>(user_cookie);
                    return pThis->m_bus.SendFrame(id, std::span<const uint8_t> {data, len});
                },
                *m_controller_id, // Receiver controller ID
                0x02,             // Sender ID
                this);

            vesc_set_response_callback([](uint8_t controller_id,
                                          uint8_t command,
                                          const uint8_t* data,
                                          uint8_t len,
                                          void* user_cookie) {
                auto pThis = static_cast<VescCanBusHandler*>(user_cookie);
                pThis->VescResponseCallback(controller_id, command, data, len);
            });


            vesc_get_values_setup(*m_controller_id);

            m_periodic_timer = StartTimer(200ms, [this]() {
                vesc_get_values_setup_selective(*m_controller_id,
                                                SETUP_VALUE_SPEED | SETUP_VALUE_ODOMETER |
                                                    SETUP_VALUE_INPUT_VOLTAGE_FILTERED);
                return 144ms;
            });

            // Set the can bus as active once the first selective values have been received
            m_start_timer = StartTimer(300ms, [this]() {
                std::optional<milliseconds> out = 100ms;

                // Wait for the mcconf state to be valid until marking the can bus as active
                if (m_vesc_can_state->mcconf)
                {
                    SetMaxSpeed(m_state.Get<AS::configuration>()->profile);
                    m_state.CheckoutReadWrite().Set<AS::can_bus_active>(true);
                    out = std::nullopt;
                }
                else
                {
                    vesc_get_mcconf_temp(*m_controller_id);
                }

                return out;
            });
        }


        vesc_process_can_frame(frame->Id(), d.data(), static_cast<uint8_t>(d.size()));
    }


    if (m_controller_id && m_vesc_can_state)
    {
        // Controller now known and mcconf is valid
        auto& co = m_state_cache.Pull();
        co.OnChangedValue<AS::configuration>([this](auto& old_conf, auto& new_conf) {
            {
                if (old_conf.profile != new_conf.profile)
                {
                    SetMaxSpeed(new_conf.profile);
                }
            }
        });
    }

    return std::nullopt;
}

void
VescCanBusHandler::SetMaxSpeed(Profile profile)
{
    if (m_vesc_can_state)
    {
        auto max_speed_kmh = kProfileSpeedTable[std::to_underlying(profile)];

        auto mcconf = &m_vesc_can_state->mcconf.value();
        // max_speed is in km/h, but the setting is in erpm so convert
        const auto fact = ((mcconf->si_motor_poles / 2.0f) * 60.0f * mcconf->si_gear_ratio) /
                          (mcconf->si_wheel_diameter * std::numbers::pi_v<float>);
        mcconf->l_max_erpm = static_cast<float>(max_speed_kmh) * fact;

        vesc_can_set_mcconf_temp(*m_controller_id, mcconf);
    }
}

void
VescCanBusHandler::VescResponseCallback(uint8_t /*controller_id*/,
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
        AS::odometer,
        AS::current_power_w,
        AS::battery_millivolts, // Millivolts is temporary until the bms reader is done
        AS::controller_temperature,
        AS::motor_temperature,
        AS::overheated,
        AS::speed,
        AS::trip_max_speed>();

    if (command == CAN_PACKET_STATUS_3)
    {
        vesc_status_msg_3_t status;
        if (vesc_parse_status_msg_3(data, len, &status))
        {
            qw.Set<AS::wh_consumed>(m_start_consumed_wh + status.watt_hours);
            qw.Set<AS::wh_regenerated>(m_start_regen_wh + status.watt_hours_charged);
        }
    }
    else if (command == CAN_PACKET_STATUS_4)
    {
        vesc_status_msg_4_t status;
        if (vesc_parse_status_msg_4(data, len, &status))
        {
            auto amps = status.current_in;

            auto watts = ro.Get<AS::battery_millivolts>() * amps / 1000.0f;
            auto fet_temperature = static_cast<uint8_t>(status.temp_fet);

            qw.Set<AS::current_power_w>(static_cast<int16_t>(watts));
            qw.Set<AS::controller_temperature>(fet_temperature);
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
                qw.Set<AS::trip_max_speed>(std::max(ro.Get<AS::trip_max_speed>(), speed));
            }
            break;
            case vesc_setup_value_index_t::SETUP_VALUE_INPUT_VOLTAGE_FILTERED: {
                auto mv = vesc_buffer_get_float16(data, 0.01f, &index);
                qw.Set<AS::battery_millivolts>(mv);
            }
            break;
            case vesc_setup_value_index_t::SETUP_VALUE_ODOMETER:
                qw.Set<AS::odometer>(vesc_buffer_get_uint32(data, &index));
                break;

            default:
                break;
            }
        }
    }
    else if (command == COMM_GET_MCCONF_TEMP)
    {
        vesc_mcconf_t value;
        if (vesc_parse_mcconf(data, len, &value))
        {
            m_vesc_can_state->mcconf = value;
        }
    }
}
