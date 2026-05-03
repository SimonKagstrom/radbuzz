#pragma once

#include "painter.hh"
#include "user_interface.hh"

#include <etl/vector.h>

class MapScreen : public UserInterface::ScreenBase
{
public:
    MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache, uint8_t zoom);

    void SetZoom(uint8_t zoom);

private:
    void DrawRangeCircle(lv_layer_t* layer);

    void Update() final;
    void HandleInput(const Input::Event& event) final;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;
    uint8_t m_zoom;

    uint32_t m_current_icon_hash {kInvalidIconHash};
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    lv_obj_t* m_soc_label {nullptr};

    BlankAlphaImage m_position_dot {32, 32};
    lv_obj_t* m_position_dot_obj {nullptr};
    BlankAlphaImage m_speed_triangle {128, 128};
    lv_obj_t* m_speed_triangle_obj {nullptr};
    lv_obj_t* m_speed_digits_label {nullptr};
    BlankAlphaImage m_battery_indicator {24, 80};
    lv_obj_t* m_battery_indicator_obj {nullptr};
    uint8_t m_last_battery_soc {255};

    Point m_current_view_center {0, 0, m_zoom};
    os::TimerHandle m_touch_timer;

    int32_t m_last_touch_x {0};
    int32_t m_last_touch_y {0};


    static constexpr int kNumTilesX = (hal::kDisplayWidth + kTileSize - 1) / kTileSize + 1;
    static constexpr int kNumTilesY = (hal::kDisplayHeight + kTileSize - 1) / kTileSize + 1;

    etl::vector<hal::BlitOperation, kNumTilesX * kNumTilesY> m_blit_ops;
};
