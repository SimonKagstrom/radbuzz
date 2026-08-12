#pragma once

#include "user_interface.hh"
#include "listener_cookie.hh"

class OtaUpdateScreen : public UserInterface::ScreenBase
{
public:
    explicit OtaUpdateScreen(UserInterface& parent);

private:
    void Update() final;
    
    // No input, no help
    void HandleInput(const Input::Event& event) final
    {
    }

    void SetHelp(bool on) final
    {
    }

    lv_obj_t* m_label;

    std::unique_ptr<ListenerCookie> m_progress_cookie;
    std::atomic<uint8_t> m_progress {0};
};
