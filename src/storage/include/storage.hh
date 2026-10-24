#pragma once

#include "application_state.hh"
#include "hal/i_nvm.hh"
#include "pooled_thread_base.hh"


class Storage : public PooledThreadBase
{
public:
    Storage(ApplicationState& application_state, hal::INvm& nvm);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    ApplicationState& m_application_state;
    hal::INvm& m_nvm;
    ListenerCookie m_state_listener;
    ApplicationState::PartialReadOnlyCache<AS::configuration, AS::is_moving, AS::timezone>
        m_state_cache;

    bool m_tainted_by_demo_mode {false};
};
