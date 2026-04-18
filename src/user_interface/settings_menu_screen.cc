#include "settings_menu_screen.hh"

SettingsMenuScreen::SettingsMenuScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
    , m_menu_screen(std::make_unique<MenuScreen>(
          parent.GetTimerManager(), m_screen, parent.m_lvgl_input_dev, [this]() {
              m_parent.ActivateScreen(*m_parent.m_map_screen);
          }))
{
}

void
SettingsMenuScreen::Update()
{
}

void
SettingsMenuScreen::HandleInput(hal::IInput::EventType event)
{
    lv_indev_read(m_parent.m_lvgl_input_dev);
}
