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

    auto& settings_page = main.AddSubPage("Settings");
    settings_page.AddNumericEntry(
        "Max speed", {25, 120, 5}, ro.Get<AS::configuration>()->max_speed, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .max_speed = value;
        });
    settings_page.AddNumericEntry("Battery cell series",
                                  {1, 36, 1},
                                  ro.Get<AS::configuration>()->battery_cell_series,
                                  [this](auto value) {
                                      m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                                          .GetWritableReference<AS::configuration>()
                                          .battery_cell_series = value;
                                  });

    main.AddSeparator();
    main.AddEntry("Reset trip", []() { printf("Resetting trip, but NYI\n"); });
    main.AddSeparator();
    main.AddBooleanEntry("Toggle demo mode", ro.Get<AS::demo_mode>(), [this](auto value) {
        m_parent.m_state.CheckoutReadWrite().Set<AS::demo_mode>(value);
    });
}

void
SettingsMenuScreen::Update()
{
}

void
SettingsMenuScreen::HandleInput(const Input::Event &event)
{
    lv_indev_read(m_parent.m_lvgl_input_dev);
    m_menu_screen->BumpExitTimer();
}
