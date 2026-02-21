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


class UserInterface : public os::BaseThread
{
public:
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

    lv_obj_t* m_screen;
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};
    lv_indev_t* m_lvgl_input_dev {nullptr};

    std::unique_ptr<MenuScreen> m_menu_screen;

    // Maybe TMP
    std::unique_ptr<uint8_t[]> m_static_map_buffer;
    std::unique_ptr<Image> m_static_map_image;
};
