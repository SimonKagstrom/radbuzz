#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_display.hh"
#include "hal/i_pm.hh"
#include "image_cache.hh"
#include "menu_screen.hh"
#include "tile_cache.hh"
#include "wgs84_to_osm_point.hh"

#include <lvgl.h>

class MapScreen;
class TripMeterScreen;

class UserInterface : public os::BaseThread
{
public:
    friend class MapScreen;
    friend class TripMeterScreen;

    class ScreenBase
    {
    public:
        explicit ScreenBase(UserInterface& parent)
            : m_parent(parent)
        {
        }

        virtual ~ScreenBase() = default;

        virtual void Update() = 0;

        void Activate()
        {
            lv_screen_load(m_screen);
        }

        //virtual void HandleInput() = 0;

        lv_obj_t* GetLvglObj()
        {
            return m_screen;
        }

    protected:
        UserInterface& m_parent;

        lv_obj_t* m_screen {nullptr};
    };

    UserInterface(hal::IDisplay& display,
                  std::unique_ptr<hal::IPm::ILock> pm_lock,
                  ApplicationState& state,
                  ImageCache& cache,
                  TileCache& tile_cache);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    hal::IDisplay& m_display;
    std::unique_ptr<hal::IPm::ILock> m_pm_lock;
    ApplicationState& m_state;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;

    lv_display_t* m_lvgl_display {nullptr};
    uint32_t m_next_redraw_time {0};


    std::unique_ptr<ListenerCookie> m_state_listener;
    std::unique_ptr<ListenerCookie> m_cache_listener;

    uint32_t m_current_icon_hash {kInvalidIconHash};

    lv_indev_t* m_lvgl_input_dev {nullptr};

    std::unique_ptr<ScreenBase> m_map_screen;
    std::unique_ptr<ScreenBase> m_trip_meter_screen;
    std::unique_ptr<MenuScreen> m_menu_screen;

    ScreenBase* m_current_screen {nullptr};
};
