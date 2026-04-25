#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_blitter.hh"
#include "hal/i_display.hh"
#include "hal/i_gpio.hh"
#include "hal/i_input.hh"
#include "hal/i_pm.hh"
#include "image_cache.hh"
#include "menu_screen.hh"
#include "tile_cache.hh"
#include "wgs84_to_osm_point.hh"

#include <etl/queue_spsc_atomic.h>
#include <lvgl.h>

class MapScreen;
class TripMeterScreen;
class SettingsMenuScreen;

class UserInterface : public os::BaseThread
{
public:
    friend class MapScreen;
    friend class TripMeterScreen;
    friend class SettingsMenuScreen;

    class ScreenBase
    {
    public:
        explicit ScreenBase(UserInterface& parent, lv_obj_t *screen)
            : m_parent(parent)
            , m_screen(screen)
        {
        }

        virtual ~ScreenBase() = default;

        virtual void Update() = 0;

        void Activate()
        {
            // Too slow for now
            //lv_screen_load_anim(m_screen, LV_SCREEN_LOAD_ANIM_FADE_OUT, 200, 0, false);
            lv_screen_load(m_screen);
        }

        virtual void HandleInput(hal::IInput::EventType event) = 0;

        lv_obj_t* GetLvglObj()
        {
            return m_screen;
        }

    protected:
        UserInterface& m_parent;

        lv_obj_t* m_screen {nullptr};
    };

    UserInterface(hal::IDisplay& display,
                  hal::IBlitter& blitter,
                  std::unique_ptr<hal::IPm::ILock> pm_lock,
                  hal::IInput& input,
                  ApplicationState& state,
                  ImageCache& cache,
                  TileCache& tile_cache);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;


    void ActivateScreen(ScreenBase& screen)
    {
        screen.Activate();
        m_current_screen = &screen;
    }

    hal::IDisplay& m_display;
    hal::IBlitter& m_blitter;
    std::unique_ptr<hal::IPm::ILock> m_pm_lock;
    hal::IInput& m_input;

    ApplicationState& m_state;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;

    ApplicationState::PartialReadOnlyCache<AS::position> m_state_cache;

    lv_display_t* m_lvgl_display {nullptr};
    uint32_t m_next_redraw_time {0};


    etl::queue_spsc_atomic<hal::IInput::EventType, 4> m_input_queue;
    int16_t m_enc_diff {0};
    lv_indev_state_t m_button_state {LV_INDEV_STATE_RELEASED};

    std::unique_ptr<ListenerCookie> m_state_listener;
    std::unique_ptr<ListenerCookie> m_cache_listener;
    std::unique_ptr<ListenerCookie> m_input_listener;

    os::TimerHandle m_menu_destructor;

    uint32_t m_current_icon_hash {kInvalidIconHash};

    lv_indev_t* m_lvgl_input_dev {nullptr};

    std::unique_ptr<ScreenBase> m_map_screen;
    std::unique_ptr<ScreenBase> m_trip_meter_screen;
    std::unique_ptr<ScreenBase> m_settings_menu_screen;

    ScreenBase* m_current_screen {nullptr};
};
