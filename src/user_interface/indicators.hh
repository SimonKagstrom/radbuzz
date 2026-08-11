#pragma once

#include "radbuzz_font_22.h"
#include "user_interface.hh"

class BatteryIndicator final : public UserInterface::IndicatorBase
{
public:
    BatteryIndicator(UserInterface& parent, const Point& position)
        : UserInterface::IndicatorBase(parent, position)
        , m_bms_label(lv_label_create(lv_layer_top()))
    {
        lv_obj_set_style_text_font(m_bms_label, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(m_bms_label, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_label_set_recolor(m_bms_label, true);
        lv_label_set_text(m_bms_label, "•");

        lv_obj_align_to(m_bms_label, m_indicator_label, LV_ALIGN_OUT_LEFT_MID, -4, -4);
    }

    void Update(ApplicationState& state) final
    {
        const uint8_t battery_soc = state.Get<AS::battery_soc>();
        const char* bms_text = "#4CAF50 •#";

        if (battery_soc > 90)
        {
            m_text = std::format("#4CAF50 {}#", LV_SYMBOL_BATTERY_FULL);
        }
        else if (battery_soc > 75)
        {
            m_text = std::format("#4CAF50 {}#", LV_SYMBOL_BATTERY_3);
        }
        else if (battery_soc >= 40)
        {
            m_text = std::format("#4CAF50 {}#", LV_SYMBOL_BATTERY_2);
        }
        else if (battery_soc > 20)
        {
            m_text = std::format("#ffa500 {}#", LV_SYMBOL_BATTERY_1);
            bms_text = "#ffa500 •#";
        }
        else
        {
            m_text = std::format("#F44336 {}#", LV_SYMBOL_BATTERY_EMPTY);
            bms_text = "#F44336 •#";
        }
        lv_label_set_text(m_indicator_label, m_text.c_str());
        lv_label_set_text(m_bms_label, bms_text);

        lv_obj_set_flag(m_indicator_label,
                        LV_OBJ_FLAG_HIDDEN,
                        m_parent.OnTripMeterScreen() || m_parent.OnSpeedometerScreen());
        lv_obj_set_flag(m_bms_label,
                        LV_OBJ_FLAG_HIDDEN,
                        m_parent.OnTripMeterScreen() || m_parent.OnSpeedometerScreen() ||
                            !state.Get<AS::bms_data>()->valid);
    }

private:
    std::string m_text;
    lv_obj_t* m_bms_label {nullptr};
};

class WifiIndicator final : public UserInterface::IndicatorBase
{
public:
    WifiIndicator(UserInterface& parent, const Point& position)
        : UserInterface::IndicatorBase(parent, position)
    {
        lv_label_set_text(m_indicator_label, std::format("#4CAF50 {}#", LV_SYMBOL_WIFI).c_str());
        // The icon is a bit off, so push a few pixels
        lv_obj_set_pos(m_indicator_label, position.x - 4, position.y);
    }

    void Update(ApplicationState& state) final
    {
        lv_obj_set_flag(m_indicator_label, LV_OBJ_FLAG_HIDDEN, !state.Get<AS::wifi_connected>());
    }
};


class BluetoothIndicator final : public UserInterface::IndicatorBase
{
public:
    BluetoothIndicator(UserInterface& parent, const Point& position)
        : UserInterface::IndicatorBase(parent, position)
    {
        lv_label_set_text(m_indicator_label,
                          std::format("#4CAF50 {}#", LV_SYMBOL_BLUETOOTH).c_str());
        // Also off, push
        lv_obj_set_pos(m_indicator_label, position.x + 2, position.y);
    }

    void Update(ApplicationState& state) final
    {
        lv_obj_set_flag(
            m_indicator_label, LV_OBJ_FLAG_HIDDEN, !state.Get<AS::bluetooth_connected>());
    }
};


class PausedIndicator final : public UserInterface::IndicatorBase
{
public:
    PausedIndicator(UserInterface& parent, const Point& position)
        : UserInterface::IndicatorBase(parent, position)
    {
        lv_label_set_text(m_indicator_label, std::format("#4CAF50 {}#", LV_SYMBOL_PAUSE).c_str());
    }

    void Update(ApplicationState& state) final
    {
        lv_obj_set_flag(m_indicator_label, LV_OBJ_FLAG_HIDDEN, state.Get<AS::is_moving>());
    }
};


class GpsLostIndicator final : public UserInterface::IndicatorBase
{
public:
    GpsLostIndicator(UserInterface& parent, const Point& position)
        : UserInterface::IndicatorBase(parent, position)
    {
        lv_label_set_text(m_indicator_label, std::format("#ffa500 {}# ", LV_SYMBOL_GPS).c_str());
    }

    void Update(ApplicationState& state) final
    {
        lv_obj_set_flag(m_indicator_label, LV_OBJ_FLAG_HIDDEN, state.Get<AS::gps_position_valid>());
    }
};


class OverheatedIndicator final : public UserInterface::IndicatorBase
{
public:
    OverheatedIndicator(UserInterface& parent, const Point& position)
        : UserInterface::IndicatorBase(parent, position)
    {
        lv_label_set_text(m_indicator_label,
                          std::format("#F44336 {}# ", LV_SYMBOL_WARNING).c_str());
    }

    void Update(ApplicationState& state) final
    {
        lv_obj_set_flag(m_indicator_label, LV_OBJ_FLAG_HIDDEN, !state.Get<AS::overheated>());
    }
};

class HomeIndicator final : public UserInterface::IndicatorBase
{
public:
    using UserInterface::IndicatorBase::IndicatorBase;

    void Update(ApplicationState& state) final
    {
        auto range = state.Get<AS::estimated_range_km>() * 1000.0f;
        bool hide = false;

        if (m_parent.m_distance_home_meters / range > 1.0f)
        {
            m_text = std::format("#F44336 {}# ", LV_SYMBOL_HOME);
        }
        else if (m_parent.m_distance_home_meters / range > 0.75f)
        {
            m_text = std::format("#ffa500 {}# ", LV_SYMBOL_HOME);
        }
        else
        {
            hide = true;
        }

        lv_obj_set_flag(m_indicator_label, LV_OBJ_FLAG_HIDDEN, hide);
        lv_label_set_text(m_indicator_label, m_text.c_str());
    }

private:
    std::string m_text;
};
