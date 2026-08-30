#include "settings_menu_screen.hh"

#include <array>
#include <radbuzz_font_22.h>
#include <string_view>

constexpr auto kSpeedometerTypeOptions = std::to_array<std::string_view>({
    "Analog",
    "Digital",
    "Analog + Digital",
});
static_assert(std::to_underlying(SpeedometerType::kAnalog) == 0);
static_assert(std::to_underlying(SpeedometerType::kDigital) == 1);
static_assert(std::to_underlying(SpeedometerType::kBoth) == 2);

constexpr auto kProfileOptions = std::to_array<std::string_view>({
    "Walk assist",
    "Moped 25 km/h",
    "Moped 30 km/h",
    "Moped 45 km/h",
    "Motorcycle",
});
static_assert(kProfileOptions.size() == static_cast<size_t>(Profile::kValueCount));
static_assert(std::to_underlying(Profile::kWalking) == 0);
static_assert(std::to_underlying(Profile::kMoped25) == 1);
static_assert(std::to_underlying(Profile::kMoped30) == 2);
static_assert(std::to_underlying(Profile::kMoped45) == 3);
static_assert(std::to_underlying(Profile::kNoLimit) == 4);

constexpr auto kHistogramModeOptions = std::to_array<std::string_view>({
    "Power",
    "Consumption",
});
static_assert(std::to_underlying(HistogramMode::kPower) == 0);
static_assert(std::to_underlying(HistogramMode::kConsumption) == 1);


SettingsMenuScreen::SettingsMenuScreen(UserInterface& parent)
    : ScreenBase(parent, lv_obj_create(nullptr))
{
}

void
SettingsMenuScreen::OnActivation()
{
    // Create on activation (since it needs quite a bit of memory)
    auto current_screen = m_parent.m_current_screen;
    m_menu_screen = std::make_unique<MenuScreen>(
        m_parent.GetTimerManager(), m_screen, m_parent.m_lvgl_input_dev, [this, current_screen]() {
            m_parent.ActivateScreen(*current_screen);
        });

    auto& main = m_menu_screen->GetMainPage();
    auto ro = m_parent.m_state.CheckoutReadonly();

    auto& settings_page = main.AddSubPage("Settings");
    main.AddSeparator();

    auto& temperature_limits = settings_page.AddSubPage("Overheating limits");
    {
        temperature_limits.AddNumericEntry(
            "Motor (°C)",
            {30, 120, 5},
            ro.Get<AS::configuration>()->motor_overheat_temperature,
            [this](auto value) {
                m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                    .GetWritableReference<AS::configuration>()
                    .motor_overheat_temperature = static_cast<uint8_t>(value);
            });
        temperature_limits.AddNumericEntry(
            "Controller (°C)",
            {30, 120, 5},
            ro.Get<AS::configuration>()->controller_overheat_temperature,
            [this](auto value) {
                m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                    .GetWritableReference<AS::configuration>()
                    .controller_overheat_temperature = static_cast<uint8_t>(value);
            });
        temperature_limits.AddNumericEntry(
            "BMS (°C)",
            {30, 120, 5},
            ro.Get<AS::configuration>()->bms_overheat_temperature,
            [this](auto value) {
                m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                    .GetWritableReference<AS::configuration>()
                    .bms_overheat_temperature = static_cast<uint8_t>(value);
            });
        temperature_limits.AddNumericEntry(
            "Battery cells (°C)",
            {30, 120, 5},
            ro.Get<AS::configuration>()->cell_overheat_temperature,
            [this](auto value) {
                m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                    .GetWritableReference<AS::configuration>()
                    .cell_overheat_temperature = static_cast<uint8_t>(value);
            });
    }


    if constexpr (false)
    {
        // Don't display this until we actually have an analogue speedometer
        settings_page.AddNumericEntry("Max speedometer speed",
                                      {25, 120, 5},
                                      ro.Get<AS::configuration>()->max_speedometer_speed,
                                      [this](auto value) {
                                          m_parent.m_state
                                              .CheckoutPartialSnapshot<AS::configuration>()
                                              .GetWritableReference<AS::configuration>()
                                              .max_speedometer_speed = static_cast<uint8_t>(value);
                                      });
    }
    settings_page.AddRollerEntry(
        "Profile",
        std::span<const std::string_view>(kProfileOptions),
        kProfileOptions[std::to_underlying(ro.Get<AS::configuration>()->profile)],
        [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .profile = static_cast<Profile>(value);
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
    settings_page.AddNumericEntry("Wh per km for range estimation",
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


    if constexpr (false)
    {
        // Don't display this until we actually have an analogue speedometer
        settings_page.AddRollerEntry(
            "Speedometer",
            std::span<const std::string_view>(kSpeedometerTypeOptions),
            kSpeedometerTypeOptions[std::to_underlying(
                ro.Get<AS::configuration>()->speedometer_type)],
            [this](auto value) {
                m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                    .GetWritableReference<AS::configuration>()
                    .speedometer_type = static_cast<SpeedometerType>(value);
            });
    }
    settings_page.AddRollerEntry(
        "Histogram display",
        std::span<const std::string_view>(kHistogramModeOptions),
        kHistogramModeOptions[std::to_underlying(ro.Get<AS::configuration>()->histogram_mode)],
        [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .histogram_mode = static_cast<HistogramMode>(value);
        });
    settings_page.AddNumericEntry("Histogram distance (meters)",
                                  {25, 1000, 25},
                                  ro.Get<AS::configuration>()->recent_power_distance,
                                  [this](auto value) {
                                      m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                                          .GetWritableReference<AS::configuration>()
                                          .recent_power_distance = static_cast<uint16_t>(value);
                                  });
    settings_page.AddBooleanEntry(
        "Show GPS speed", ro.Get<AS::configuration>()->show_gps_speed, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .show_gps_speed = value;
        });
    settings_page.AddBooleanEntry(
        "Rotate map with heading", ro.Get<AS::configuration>()->rotate_map, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .rotate_map = value;
        });
    settings_page.AddBooleanEntry("Show speech bubbles at start",
                                  ro.Get<AS::configuration>()->show_speech_bubbles,
                                  [this](auto value) {
                                      m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                                          .GetWritableReference<AS::configuration>()
                                          .show_speech_bubbles = value;
                                  });
    settings_page.AddBooleanEntry(
        "Force C6 FW upgrade", ro.Get<AS::configuration>()->force_c6_update, [this](auto value) {
            m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
                .GetWritableReference<AS::configuration>()
                .force_c6_update = value;
        });

    main.AddEntry("Reset trip", [this]() {
        m_parent.ResetTrip();
        m_menu_screen->ExitMenu();
    });
    main.AddSeparator();
    main.AddBooleanEntry("Show help text", m_parent.m_help_enabled, [this](auto value) {
        m_parent.m_help_enabled = value;
    });
    main.AddBooleanEntry("Demo mode", ro.Get<AS::demo_mode>(), [this](auto value) {
        m_parent.m_state.CheckoutReadWrite().Set<AS::demo_mode>(value);
    });


    m_odometer_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_odometer_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_align(m_odometer_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(m_odometer_label, LV_ALIGN_TOP_MID, 0, 0);

    m_consumed_regen_label = lv_label_create(m_screen);

    lv_obj_set_style_text_font(m_consumed_regen_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_align(m_consumed_regen_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    lv_obj_align_to(m_consumed_regen_label, m_odometer_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);


    // No help while in the menu
    m_parent.HideHelp();
}

void
SettingsMenuScreen::OnDeactivation()
{
    lv_obj_del(m_odometer_label);
    lv_obj_del(m_consumed_regen_label);

    m_menu_screen = nullptr;
    if (m_parent.m_help_enabled)
    {
        m_parent.ShowHelp();
    }
}

void
SettingsMenuScreen::Update()
{
    auto ro = m_parent.m_state.CheckoutReadonly();
    auto odometer_km = ro.Get<AS::odometer>() / 1000.0f;
    auto consumed_kwh = ro.Get<AS::wh_consumed>() / 1000.0f;
    auto regen_kwh = ro.Get<AS::wh_regenerated>() / 1000.0f;

    lv_label_set_text(m_odometer_label, std::format("Odometer: {:.1f} km", odometer_km).c_str());
    lv_label_set_text(m_consumed_regen_label,
                      std::format("Consumed: -{:.1f}+{:.1f} kWh", consumed_kwh, regen_kwh).c_str());
    lv_obj_align_to(m_consumed_regen_label, m_odometer_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
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

void
SettingsMenuScreen::SetHelp(bool on)
{
    // No help for now
}
