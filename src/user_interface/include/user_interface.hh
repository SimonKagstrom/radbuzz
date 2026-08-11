#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "digital_speedometer_widget.hh"
#include "hal/i_blitter.hh"
#include "hal/i_display.hh"
#include "hal/i_gpio.hh"
#include "hal/i_input.hh"
#include "hal/i_pm.hh"
#include "image_cache.hh"
#include "input.hh"
#include "menu_screen.hh"
#include "speech_bubble.hh"
#include "tile_cache.hh"
#include "trip_computer.hh"
#include "wgs84_to_osm_point.hh"

#include <etl/queue_spsc_atomic.h>
#include <lvgl.h>

class MapScreen;
class TripMeterScreen;
class SettingsMenuScreen;
class SpeedometerOnlyScreen;
class HomeIndicator;

constexpr auto kPowerBarWidth = 10;

class UserInterface : public os::BaseThread
{
public:
    friend class MapScreen;
    friend class TripMeterScreen;
    friend class SettingsMenuScreen;
    friend class SpeedometerOnlyScreen;
    friend class HomeIndicator;

    class ScreenBase
    {
    public:
        explicit ScreenBase(UserInterface& parent, lv_obj_t* screen)
            : m_parent(parent)
            , m_screen(screen)
        {
        }

        virtual ~ScreenBase() = default;
        virtual void Update() = 0;
        virtual void HandleInput(const Input::Event& event) = 0;

        virtual void OnActivation()
        {
        }

        virtual void OnDeactivation()
        {
            UpdateHelp();
        }

        void UpdateHelp()
        {
            for (auto& bubble : m_explanatory_bubbles)
            {
                bubble->Update();
            }
        }

        void Activate()
        {
            // Too slow for now
            //lv_screen_load_anim(m_screen, LV_SCREEN_LOAD_ANIM_FADE_OUT, 200, 0, false);
            lv_screen_load(m_screen);
        }

        lv_obj_t* GetLvglObj()
        {
            return m_screen;
        }

        virtual void SetHelp(bool on) = 0;

    protected:
        UserInterface& m_parent;

        lv_obj_t* m_screen {nullptr};
        std::vector<std::unique_ptr<SpeechBubble>> m_explanatory_bubbles;
    };

    class IndicatorBase
    {
    public:
        friend class UserInterface;

        IndicatorBase(UserInterface& parent, const Point& position);
        virtual ~IndicatorBase() = default;

        virtual void Update(ApplicationState& state) = 0;

    protected:
        UserInterface& m_parent;
        lv_obj_t* m_indicator_label {nullptr};
    };

    UserInterface(hal::IDisplay& display,
                  hal::IBlitter& blitter,
                  std::unique_ptr<hal::IPm::ILock> pm_lock,
                  hal::IInput& input,
                  ApplicationState& state,
                  ImageCache& cache,
                  TileCache& tile_cache,
                  TripComputer& trip_computer);

    bool OnMapScreen() const
    {
        return m_current_screen == m_map_screen.get();
    }

    bool OnTripMeterScreen() const
    {
        return m_current_screen == m_trip_meter_screen.get();
    }

    bool OnSpeedometerScreen() const
    {
        return m_current_screen == m_speedometer_only_screen.get();
    }

    void ShowHelp()
    {
        SetHelp(true);
    }

    void HideHelp()
    {
        SetHelp(false);
    }

private:
    void SetHelp(bool on);

    struct CurrentTrip
    {
        float start_wh_consumed {0};
        float start_wh_regenerated {0};
    };


    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void ResetTrip();
    void DrawPowerBar(uint16_t* dst);

    void ActivateScreen(ScreenBase& screen)
    {
        if (m_show_all_indicators_timer && !m_show_all_indicators_timer->IsExpired())
        {
            // Don't allow switching until indicators have shown
            return;
        }

        screen.OnActivation();
        screen.Activate();
        if (m_current_screen)
        {
            m_current_screen->OnDeactivation();
        }
        m_current_screen = &screen;
    }

    hal::IDisplay& m_display;
    hal::IBlitter& m_blitter;

    std::unique_ptr<hal::IPm::ILock> m_pm_lock;
    hal::IInput& m_input;

    ApplicationState& m_state;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;
    TripComputer& m_trip_computer;

    ApplicationState::PartialReadOnlyCache<AS::pixel_position> m_state_cache;
    uint32_t m_distance_home_meters {0};

    CurrentTrip m_current_trip_start;

    lv_display_t* m_lvgl_display {nullptr};
    uint32_t m_next_redraw_time {0};

    std::array<hal::BlitOperation, 2> m_rotation_blit_operations {};


    etl::queue_spsc_atomic<Input::Event, 8> m_input_queue;
    int16_t m_enc_diff {0};
    lv_indev_state_t m_button_state {LV_INDEV_STATE_RELEASED};
    lv_indev_state_t m_touch_state {LV_INDEV_STATE_RELEASED};
    lv_point_t m_touch_point {0, 0};

    std::unique_ptr<ListenerCookie> m_state_listener;
    std::unique_ptr<ListenerCookie> m_cache_listener;
    std::unique_ptr<ListenerCookie> m_input_listener;

    os::TimerHandle m_trip_start_initial_timer;
    os::TimerHandle m_menu_destructor;
    os::TimerHandle m_show_all_indicators_timer;
    os::TimerHandle m_show_help_timer;

    uint32_t m_current_icon_hash {kInvalidIconHash};

    lv_indev_t* m_lvgl_input_dev {nullptr};
    lv_indev_t* m_lvgl_touch_input_dev {nullptr};

    std::unique_ptr<ScreenBase> m_map_screen;
    std::unique_ptr<ScreenBase> m_trip_meter_screen;
    std::unique_ptr<ScreenBase> m_speedometer_only_screen;
    std::unique_ptr<ScreenBase> m_settings_menu_screen;

    etl::vector<ScreenBase*, 4> m_screens;
    ScreenBase* m_current_screen {nullptr};

    std::unique_ptr<DigitalSpeedometerWidget> m_digital_speedometer;
    std::vector<std::unique_ptr<IndicatorBase>> m_indicators;
    std::vector<std::unique_ptr<SpeechBubble>> m_explanatory_bubbles;

    bool m_help_enabled {false};
};
