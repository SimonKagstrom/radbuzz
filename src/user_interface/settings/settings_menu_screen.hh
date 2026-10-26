#pragma once

#include "image.hh"
#include "menu_screen.hh"
#include "user_interface.hh"
#include "wifi_page.hh"

class SettingsMenuScreen : public UserInterface::ScreenBase
{
public:
    explicit SettingsMenuScreen(UserInterface& parent);

    // While the menu is shown
    void OnWifiScanResult(const MSG::wifi_scan_result& result);

private:
    void OnActivation() final;
    void OnDeactivation() final;
    void Update() final;
    void HandleInput(const Input::Event& event) final;
    void SetHelp(bool on) final;

    void ShowTileStyleSample(TileStyle style);

    lv_obj_t* m_odometer_label {nullptr};
    lv_obj_t* m_consumed_regen_label {nullptr};

    // Before the menu, which shows it
    std::unique_ptr<SingleColorImage> m_tile_style_sample;
    lv_obj_t* m_tile_style_sample_obj {nullptr};

    std::unique_ptr<MenuScreen> m_menu_screen;
    // After the menu, since it refers to a page in it
    std::unique_ptr<WifiPage> m_wifi_page;
};
