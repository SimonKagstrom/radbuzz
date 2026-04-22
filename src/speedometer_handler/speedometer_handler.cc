#include "speedometer_handler.hh"

SpeedometerHandler::SpeedometerHandler(hal::IStepperMotor& motor,
                                       ApplicationState& app_state,
                                       int32_t zero_to_max_steps)
    : m_motor(motor)
    , m_state(app_state)
    , m_zero_to_max_steps(zero_to_max_steps)
    , m_state_listener(m_state.AttachListener<AS::speed, AS::configuration>(GetSemaphore()))
{
}

void
SpeedometerHandler::OnStartup()
{
    // Make sure it's zero at start
    m_motor.Step(-m_zero_to_max_steps);
}

std::optional<milliseconds>
SpeedometerHandler::OnActivation()
{
    if (auto now = os::GetTimeStamp(); now - m_last_step_time < 100ms)
    {
        // Don't step too fast - round to 100ms
        return 100ms - (now - m_last_step_time);
    }

    auto ro = m_state.CheckoutReadonly();
    auto conf = ro.Get<AS::configuration>();

    if (conf->max_speed == 0)
    {
        // Configuration not valid yet
        return std::nullopt;
    }

    const int32_t target_speed = std::min(ro.Get<AS::speed>(), conf->max_speed);
    const auto target_position =
        (target_speed * m_zero_to_max_steps + (conf->max_speed / 2u)) / conf->max_speed;

    if (const auto delta = target_position - m_position; delta != 0)
    {
        m_motor.Step(delta);
    }

    m_position = target_position;

    // Take a new timestamp, since the stepping itself might take time
    m_last_step_time = os::GetTimeStamp();

    return std::nullopt;
}
