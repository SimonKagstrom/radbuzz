#include "application_state.hh"

#include <mutex>

ApplicationState::ReadOnlyState::ReadOnlyState(ApplicationState& parent)
    : m_parent(parent)
{
}


class ApplicationState::ListenerImpl : public ApplicationState::IListener
{
public:
    ListenerImpl(ApplicationState& parent, os::binary_semaphore& semaphore)
        : m_parent(parent)
        , m_semaphore(semaphore)
    {
    }

    ~ListenerImpl() final
    {
        // std::erase(m_parent.m_listeners.begin(), m_parent.m_listeners.end(), this);
    }

    void Awake()
    {
        m_semaphore.release();
    }

private:
    ApplicationState& m_parent;
    os::binary_semaphore& m_semaphore;
};

ApplicationState::ApplicationState()
{
    m_global_state.SetupDefaultValues();
}

std::unique_ptr<ApplicationState::IListener>
ApplicationState::AttachListener(os::binary_semaphore& semaphore)
{
    auto out = std::make_unique<ListenerImpl>(*this, semaphore);

    m_listeners.push_back(out.get());

    return out;
}

ApplicationState::ReadWriteState
ApplicationState::CheckoutReadWrite()
{
    return ApplicationState::ReadWriteState(*this);
}

ApplicationState::ReadOnlyState
ApplicationState::CheckoutReadonly()
{
    return ApplicationState::ReadOnlyState(*this);
}
