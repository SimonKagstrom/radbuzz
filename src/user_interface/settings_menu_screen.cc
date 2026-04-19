#include "settings_menu_screen.hh"

SettingsMenuScreen::SettingsMenuScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
    , m_menu_screen(std::make_unique<MenuScreen>(
          parent.GetTimerManager(), m_screen, parent.m_lvgl_input_dev, [this]() {
              m_parent.ActivateScreen(*m_parent.m_map_screen);
          }))
{
    auto& main = m_menu_screen->GetMainPage();
    auto ro = m_parent.m_state.CheckoutReadonly();


    main.AddSubPage("Settings");
    main.AddSeparator();
    main.AddEntry("Reset trip", [](lv_event_t*) { printf("Resetting trip, but NYI\n"); });
    main.AddSeparator();
    main.AddBooleanEntry("Toggle demo mode", ro.Get<AS::demo_mode>(), [this](lv_event_t* e) {
        auto sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
        auto checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

        m_parent.m_state.CheckoutReadWrite().Set<AS::demo_mode>(checked);
    });
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
