#include "speedometer_handler.hh"

constexpr uint8_t kMaxSpeed = 60; // At max steps we use 60km/h

SpeedometerHandler::SpeedometerHandler(hal::IStepperMotor& motor,
                                       ApplicationState& app_state,
                                       int32_t zero_to_max_steps)
    : m_motor(motor)
    , m_state(app_state)
    , m_zero_to_max_steps(zero_to_max_steps)
    , m_state_listener(m_state.AttachListener<AS::speed>(GetSemaphore()))
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

    const int32_t target_speed = std::min(m_state.CheckoutReadonly().Get<AS::speed>(), kMaxSpeed);
    const auto target_position =
        (target_speed * m_zero_to_max_steps + (kMaxSpeed / 2u)) / kMaxSpeed;

    if (const auto delta = target_position - m_position; delta != 0)
    {
        m_motor.Step(delta);
    }

    m_position = target_position;

    // Take a new timestamp, since the stepping itself might take time
    m_last_step_time = os::GetTimeStamp();

    return std::nullopt;
}
