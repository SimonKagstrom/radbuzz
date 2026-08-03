#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_nvm.hh"


class Storage : public os::BaseThread
{
public:
    Storage(ApplicationState& application_state, hal::INvm& nvm);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    ApplicationState& m_application_state;
    std::unique_ptr<ListenerCookie> m_state_listener;
    hal::INvm& m_nvm;
    ApplicationState::PartialReadOnlyCache<AS::configuration, AS::is_moving> m_state_cache;

    bool m_tainted_by_demo_mode {false};
};
