#include "settings_menu_screen.hh"

#include <array>
#include <string_view>

constexpr auto kSpeedometerTypeOptions = std::to_array<std::string_view>({
    "Analog",
    "Digital",
    "Analog + Digital",
});
static_assert(std::to_underlying(SpeedometerType::kAnalog) == 0);
static_assert(std::to_underlying(SpeedometerType::kDigital) == 1);
static_assert(std::to_underlying(SpeedometerType::kBoth) == 2);

SettingsMenuScreen::SettingsMenuScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
{
}

void
SettingsMenuScreen::OnActivation()
{
    // Create on activation (since it needs quite a bit of memory)
    m_menu_screen = std::make_unique<MenuScreen>(
        m_parent.GetTimerManager(), m_screen, m_parent.m_lvgl_input_dev, [this]() {
            m_parent.ActivateScreen(*m_parent.m_map_screen);
        });

    auto& main = m_menu_screen->GetMainPage();
    auto ro = m_parent.m_state.CheckoutReadonly();

    auto& settings_page = main.AddSubPage("Settings");
    main.AddSeparator();
    settings_page.AddNumericEntry(
        "Max speed", {25, 120, 5}, ro.Get<AS::configuration>()->max_speed, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .max_speed = static_cast<uint8_t>(value);
        });
    settings_page.AddNumericEntry("Battery cell series",
                                  {1, 36},
                                  ro.Get<AS::configuration>()->battery_cell_series,
                                  [this](auto value) {
                                      m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                                          .GetWritableReference<AS::configuration>()
                                          .battery_cell_series = static_cast<uint8_t>(value);
                                  });
    settings_page.AddNumericEntry(
        "Battery Ah", {1, 100}, ro.Get<AS::configuration>()->battery_amp_hours, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .battery_amp_hours = static_cast<uint8_t>(value);
        });
    settings_page.AddNumericEntry("Wh per km",
                                  {10, 100},
                                  ro.Get<AS::configuration>()->wh_per_km_for_range_estimation,
                                  [this](auto value) {
                                      m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                                          .GetWritableReference<AS::configuration>()
                                          .wh_per_km_for_range_estimation =
                                          static_cast<uint8_t>(value);
                                  });
    settings_page.AddNumericEntry("Motor watts",
                                  {500, 10000, 100},
                                  ro.Get<AS::configuration>()->max_watts,
                                  [this](auto value) {
                                      m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                                          .GetWritableReference<AS::configuration>()
                                          .max_watts = value;
                                  });


    settings_page.AddRollerEntry(
        "Speedometer",
        std::span<const std::string_view>(kSpeedometerTypeOptions),
        kSpeedometerTypeOptions[std::to_underlying(ro.Get<AS::configuration>()->speedometer_type)],
        [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .speedometer_type = static_cast<SpeedometerType>(value);
        });
    settings_page.AddBooleanEntry(
        "Rotate map with heading", ro.Get<AS::configuration>()->rotate_map, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .rotate_map = value;
        });
    main.AddEntry("Reset trip", []() { printf("Resetting trip, but NYI\n"); });
    main.AddSeparator();
    main.AddBooleanEntry("Toggle demo mode", ro.Get<AS::demo_mode>(), [this](auto value) {
        m_parent.m_state.CheckoutReadWrite().Set<AS::demo_mode>(value);
    });
}

void
SettingsMenuScreen::OnDeactivation()
{
    m_menu_screen = nullptr;
}

void
SettingsMenuScreen::Update()
{
}

void
SettingsMenuScreen::HandleInput(const Input::Event& event)
{
    if (m_menu_screen)
    {
        m_menu_screen->BumpExitTimer();
        lv_indev_read(m_parent.m_lvgl_input_dev);
    }
}
