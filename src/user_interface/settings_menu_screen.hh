#pragma once

#include "menu_screen.hh"
#include "user_interface.hh"

class SettingsMenuScreen : public UserInterface::ScreenBase
{
public:
    explicit SettingsMenuScreen(UserInterface& parent);

private:
    void OnActivation() final;
    void OnDeactivation() final;
    void Update() final;
    void HandleInput(const Input::Event &event) final;
    void SetHelp(bool on) final;

    lv_obj_t* m_odometer_label {nullptr};
    lv_obj_t* m_consumed_regen_label {nullptr};

    std::unique_ptr<MenuScreen> m_menu_screen;
};
