#include "wifi_connector.hh"

WifiConnector::WifiConnector(hal::IWifiClient& wifi_client,
                             os::TimerManager& timer_manager,
                             const std::atomic_bool& link_up,
                             std::atomic_bool& link_down_event,
                             WifiSsidNetwork network)
    : m_wifi_client(wifi_client)
    , m_timer_manager(timer_manager)
    , m_link_up(link_up)
    , m_link_down_event(link_down_event)
    , m_network(std::move(network))
{
}

WifiConnector::~WifiConnector()
{
    // Neither connected nor failed yet. Not in RetryConnect, since nothing is ongoing then
    if (CurrentState() == State::kConnect)
    {
        m_wifi_client.Disconnect();
    }
}

WifiConnector::Result
WifiConnector::EvaluateConnection()
{
    RunStateMachine();

    switch (CurrentState())
    {
    case State::kSucceeded:
        return Result::kSucceeded;
    case State::kFailed:
        return Result::kFailed;
    default:
        return Result::kPending;
    }
}

const std::string&
WifiConnector::Ssid() const
{
    return m_network.ssid;
}

void
WifiConnector::OnTransition(State from, State to)
{
    printf("Wifi connect %s: %s -> %s\n",
           m_network.ssid.c_str(),
           StateName(from).data(),
           StateName(to).data());
}


// Connect
void
WifiConnector::Enter(Connect& state)
{
    m_link_down_event = false;

    state.timer = m_timer_manager.StartTimer(kConnectTimeout);

    m_wifi_client.Connect(m_network.ssid.c_str(), m_network.password.c_str());
}

WifiConnector::Connect::Next
WifiConnector::Evaluate(Connect& state)
{
    if (m_link_up)
    {
        return State::kSucceeded;
    }

    if (!m_link_down_event && !state.timer->IsExpired())
    {
        return kStay;
    }

    if (m_retries >= kMaxConnectRetries)
    {
        return State::kFailed;
    }

    if (m_link_down_event)
    {
        return State::kRetryConnect;
    }

    return State::kAbort;
}


// Abort
void
WifiConnector::Enter(Abort&)
{
    m_wifi_client.Disconnect();
}

WifiConnector::Abort::Next
WifiConnector::Evaluate(Abort&)
{
    return State::kRetryConnect;
}


// RetryConnect
void
WifiConnector::Enter(RetryConnect& state)
{
    m_retries++;
    state.timer = m_timer_manager.StartTimer(kRetryConnectTime,
                                             []() { return std::optional<milliseconds>(); });
}

WifiConnector::RetryConnect::Next
WifiConnector::Evaluate(RetryConnect& state)
{
    if (state.timer->IsExpired())
    {
        return State::kConnect;
    }

    return kStay;
}


// Succeeded and Failed are final, the wifi handler acts on them
WifiConnector::Succeeded::Next
WifiConnector::Evaluate(Succeeded&)
{
    return kStay;
}

void
WifiConnector::Enter(Failed&)
{
    // Still ongoing after a timeout
    m_wifi_client.Disconnect();
}

WifiConnector::Failed::Next
WifiConnector::Evaluate(Failed&)
{
    return kStay;
}
