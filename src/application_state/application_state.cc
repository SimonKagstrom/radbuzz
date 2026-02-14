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
        m_parent.DetachListener(this);
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
ApplicationState::DoAttachListener(const etl::bitset<AS::kLastIndex + 1, uint32_t>& interested,
                                   os::binary_semaphore& semaphore)
{
    auto out = std::make_unique<ListenerImpl>(*this, semaphore);

    for (auto index = interested.find_first(true); index != interested.npos;
         index = interested.find_next(true, index + 1))
    {
        m_listeners[index].push_back(out.get());
    }

    return out;
}

void
ApplicationState::DetachListener(const ListenerImpl* impl)
{
    std::lock_guard lock(m_mutex);

    for (auto& listeners : m_listeners)
    {
        std::erase_if(listeners, [impl](const auto* listener) { return listener == impl; });
    }
}

void
ApplicationState::NotifyChange(unsigned index)
{
    for (auto listener : m_listeners[index])
    {
        listener->Awake();
    }
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