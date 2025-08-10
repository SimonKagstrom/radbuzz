#include "application_state.hh"

#include <mutex>

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

struct ApplicationState::StateImpl : ApplicationState::State
{
    explicit StateImpl(ApplicationState& parent)
        : m_parent(parent)
        , m_shadow(parent.m_global_state)
    {
        static_cast<State&>(*this) = parent.m_global_state;
    }

    ~StateImpl() final
    {
        m_parent.Commit(this);
    }

    ApplicationState& m_parent;
    ApplicationState::State m_shadow;
};


std::unique_ptr<ApplicationState::IListener>
ApplicationState::AttachListener(os::binary_semaphore& semaphore)
{
    auto out = std::make_unique<ListenerImpl>(*this, semaphore);

    m_listeners.push_back(out.get());

    return out;
}

std::unique_ptr<ApplicationState::State>
ApplicationState::Checkout()
{
    std::lock_guard lock(m_mutex);

    return std::make_unique<StateImpl>(*this);
}

const ApplicationState::State*
ApplicationState::CheckoutReadonly() const
{
    return &m_global_state;
}


template <typename M>
bool
ApplicationState::UpdateIfChanged(M ApplicationState::State::* member,
                                  const ApplicationState::StateImpl* current_state,
                                  ApplicationState::State* global_state) const
{
    if (current_state->*member != current_state->m_shadow.*member)
    {
        global_state->*member = current_state->*member;
        return true;
    }

    return false;
}

void
ApplicationState::Commit(const ApplicationState::StateImpl* state)
{
    std::lock_guard lock(m_mutex);

    auto updated = false;

    // Ugly, but reflection is not available
    updated |= UpdateIfChanged(&ApplicationState::State::navigation_active, state, &m_global_state);
    updated |= UpdateIfChanged(&ApplicationState::State::current_icon_hash, state, &m_global_state);
    updated |= UpdateIfChanged(&ApplicationState::State::distance_to_next, state, &m_global_state);

    // Make sure the backing store reflects the next street, so make a copy but leave the old lingering
    if (state->m_shadow.next_street != m_next_street[m_active_street])
    {
        m_next_street[m_active_street] = state->m_shadow.next_street;
        m_global_state.next_street = m_next_street[m_active_street];

        m_active_street = !m_active_street;
        updated = true;
    }

    if (!updated)
    {
        return;
    }

    for (auto listener : m_listeners)
    {
        listener->Awake();
    }
}
