#include "settings_menu_screen.hh"

SettingsMenuScreen::SettingsMenuScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
    , m_menu_screen(std::make_unique<MenuScreen>(
          parent.GetTimerManager(), m_screen, parent.m_lvgl_input_dev, [this]() {
              m_parent.ActivateScreen(*m_parent.m_map_screen);
          }))
{
    auto& main = m_menu_screen->GetMainPage();
    main.AddSubPage("Settings");
    main.AddSeparator();
    main.AddEntry("Reset trip", [](lv_event_t*) { printf("Resetting trip, but NYI\n"); });
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
