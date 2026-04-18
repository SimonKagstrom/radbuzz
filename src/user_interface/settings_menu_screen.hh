#pragma once

#include "menu_screen.hh"
#include "user_interface.hh"

class SettingsMenuScreen : public UserInterface::ScreenBase
{
public:
    explicit SettingsMenuScreen(UserInterface& parent);

private:
    void Update() final;
    void HandleInput(hal::IInput::EventType event) final;
    std::unique_ptr<MenuScreen> m_menu_screen;
};
