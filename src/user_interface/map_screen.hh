#pragma once

#include "painter.hh"
#include "user_interface.hh"

#include <etl/vector.h>

class MapScreen : public UserInterface::ScreenBase
{
public:
    MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache);

private:
    void Update() final;
    void HandleInput(const Input::Event &event) final;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;

    uint32_t m_current_icon_hash {kInvalidIconHash};
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    lv_obj_t* m_soc_label {nullptr};

    BlankAlphaImage m_position_dot {32, 32};
    lv_obj_t* m_position_dot_obj {nullptr};

    Point m_current_view_center {0, 0, kDefaultZoom};

    static constexpr int kNumTilesX = (hal::kDisplayWidth + kTileSize - 1) / kTileSize + 1;
    static constexpr int kNumTilesY = (hal::kDisplayHeight + kTileSize - 1) / kTileSize + 1;

    etl::vector<hal::BlitOperation, kNumTilesX * kNumTilesY> m_blit_ops;
};
