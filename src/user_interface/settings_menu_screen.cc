#include "settings_menu_screen.hh"

#include "png_decoder.hh"
#include "tile_styles.hh"

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

namespace
{

template <typename Field>
auto
ConfigurationSetter(ApplicationState& state, Field ConfigurationSettings::* field)
{
    return [&state, field](auto value) {
        auto ps = state.CheckoutPartialSnapshot<AS::configuration>();
        ps.GetWritableReference<AS::configuration>().*field = static_cast<Field>(value);
    };
}

} // namespace


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
    const auto conf = m_parent.m_state.Get<AS::configuration>();

    auto& settings_page = main.AddSubPage("Settings");
    main.AddSeparator();

    auto& temperature_limits = settings_page.AddSubPage("Overheating limits");
    {
        temperature_limits.AddNumericEntry(
            "Motor (°C)",
            {30, 120, 5},
            conf->motor_overheat_temperature,
            ConfigurationSetter(m_parent.m_state,
                                &ConfigurationSettings::motor_overheat_temperature));
        temperature_limits.AddNumericEntry(
            "Controller (°C)",
            {30, 120, 5},
            conf->controller_overheat_temperature,
            ConfigurationSetter(m_parent.m_state,
                                &ConfigurationSettings::controller_overheat_temperature));
        temperature_limits.AddNumericEntry(
            "BMS (°C)",
            {30, 120, 5},
            conf->bms_overheat_temperature,
            ConfigurationSetter(m_parent.m_state,
                                &ConfigurationSettings::bms_overheat_temperature));
        temperature_limits.AddNumericEntry(
            "Battery cells (°C)",
            {30, 120, 5},
            conf->cell_overheat_temperature,
            ConfigurationSetter(m_parent.m_state,
                                &ConfigurationSettings::cell_overheat_temperature));
    }

    auto& map_style_page = settings_page.AddSubPage("Map style");
    {
        std::array<std::string_view, std::to_underlying(TileStyle::kValueCount)> names;
        for (auto i = 0u; i < names.size(); i++)
        {
            names[i] = TileStyleName(static_cast<TileStyle>(i));
        }

        m_tile_style_sample = std::make_unique<SingleColorImage>(kTileSize, kTileSize, 2, 0);
        ShowTileStyleSample(conf->tile_style);

        // Show the sample of each style when scrolling through them
        map_style_page.AddRollerEntry(
            "Style",
            std::span<const std::string_view>(names),
            TileStyleName(conf->tile_style),
            ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::tile_style),
            [this](auto which) { ShowTileStyleSample(static_cast<TileStyle>(which)); });
        m_tile_style_sample_obj = map_style_page.AddImage(&m_tile_style_sample->lv_image_dsc);
    }


    if constexpr (false)
    {
        // Don't display this until we actually have an analogue speedometer
        settings_page.AddNumericEntry(
            "Max speedometer speed",
            {25, 120, 5},
            conf->max_speedometer_speed,
            ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::max_speedometer_speed));
    }
    settings_page.AddRollerEntry(
        "Profile",
        std::span<const std::string_view>(kProfileOptions),
        kProfileOptions[std::to_underlying(conf->profile)],
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::profile));
    settings_page.AddNumericEntry(
        "Battery cell series",
        {1, 36},
        conf->battery_cell_series,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::battery_cell_series));
    settings_page.AddNumericEntry(
        "Battery Ah",
        {1, 100},
        conf->battery_amp_hours,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::battery_amp_hours));
    settings_page.AddNumericEntry(
        "Wh per km for range estimation",
        {10, 100},
        conf->wh_per_km_for_range_estimation,
        ConfigurationSetter(m_parent.m_state,
                            &ConfigurationSettings::wh_per_km_for_range_estimation));
    settings_page.AddNumericEntry(
        "Motor watts",
        {500, 10000, 100},
        conf->max_watts,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::max_watts));


    if constexpr (false)
    {
        // Don't display this until we actually have an analogue speedometer
        settings_page.AddRollerEntry(
            "Speedometer",
            std::span<const std::string_view>(kSpeedometerTypeOptions),
            kSpeedometerTypeOptions[std::to_underlying(conf->speedometer_type)],
            ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::speedometer_type));
    }
    settings_page.AddRollerEntry(
        "Histogram display",
        std::span<const std::string_view>(kHistogramModeOptions),
        kHistogramModeOptions[std::to_underlying(conf->histogram_mode)],
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::histogram_mode));
    settings_page.AddNumericEntry(
        "Histogram distance (meters)",
        {25, 1000, 25},
        conf->recent_power_distance,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::recent_power_distance));
    settings_page.AddBooleanEntry(
        "Show GPS speed",
        conf->show_gps_speed,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::show_gps_speed));
    settings_page.AddBooleanEntry(
        "Show clock",
        conf->show_clock,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::show_clock));
    settings_page.AddBooleanEntry(
        "Rotate map with heading",
        conf->rotate_map,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::rotate_map));
    settings_page.AddBooleanEntry(
        "Show speech bubbles at start",
        conf->show_speech_bubbles,
        ConfigurationSetter(m_parent.m_state, &ConfigurationSettings::show_speech_bubbles));
    settings_page.AddSeparator();
    settings_page.AddEntry("Perform OTA update", [this]() {
        m_parent.m_state.Set<AS::ota_update_active>(true);
        m_parent.m_state.Set<AS::demo_mode>(false);

        m_parent.ActivateScreen(*m_parent.m_ota_update_screen);
    });

    main.AddEntry("Reset trip", [this]() {
        m_parent.ResetTrip();
        m_menu_screen->ExitMenu();
    });
    main.AddSeparator();
    main.AddBooleanEntry("Show help text", m_parent.m_help_enabled, [this](auto value) {
        m_parent.m_help_enabled = value;
    });
    main.AddBooleanEntry("Demo mode", m_parent.m_state.Get<AS::demo_mode>(), [this](auto value) {
        m_parent.m_state.Set<AS::demo_mode>(value);
    });
    main.AddSeparator();
    main.AddEntry("About", [this]() {
        m_parent.m_post_office.Send<MSG::show_about>();
        m_menu_screen->ExitMenu();
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
    m_tile_style_sample_obj = nullptr;
    m_tile_style_sample = nullptr;

    if (m_parent.m_help_enabled)
    {
        m_parent.ShowHelp();
    }
}

void
SettingsMenuScreen::Update()
{
    auto odometer_km = m_parent.m_state.Get<AS::odometer>() / 1000.0f;
    auto consumed_kwh = m_parent.m_state.Get<AS::wh_consumed>() / 1000.0f;
    auto regen_kwh = m_parent.m_state.Get<AS::wh_regenerated>() / 1000.0f;

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
SettingsMenuScreen::ShowTileStyleSample(TileStyle style)
{
    if (!DecodePng(TileStyleSample(style), *m_tile_style_sample))
    {
        // Shouldn't happen, since they are built in
        std::fill_n(m_tile_style_sample->WritableData16(), m_tile_style_sample->Data16().size(), 0);
    }

    if (m_tile_style_sample_obj)
    {
        lv_obj_invalidate(m_tile_style_sample_obj);
    }
}

void
SettingsMenuScreen::SetHelp(bool on)
{
    // No help for now
}
