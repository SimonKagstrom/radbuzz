#pragma once

#include "application_state.hh"
#include "base_thread.hh"
#include "hal/i_display.hh"
#include "image_cache.hh"
#include "tile_cache.hh"

#include <lvgl.h>


class UserInterface : public os::BaseThread
{
public:
    UserInterface(hal::IDisplay& display,
                  ApplicationState& state,
                  ImageCache& cache,
                  TileCache& tile_cache);

private:
    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    hal::IDisplay& m_display;
    ApplicationState& m_state;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;

    lv_display_t* m_lvgl_display {nullptr};
    uint32_t m_next_redraw_time {0};


    std::unique_ptr<ApplicationState::IListener> m_state_listener;
    std::unique_ptr<ListenerCookie> m_cache_listener;

    uint32_t m_current_icon_hash {kInvalidIconHash};

    lv_obj_t* m_screen;
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    // Maybe TMP
    std::unique_ptr<uint8_t[]> m_static_map_buffer;
    std::unique_ptr<Image> m_static_map_image;
    lv_obj_t* m_background;
};
