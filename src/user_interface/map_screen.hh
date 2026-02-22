#pragma once

#include "user_interface.hh"

class MapScreen : public UserInterface::ScreenBase
{
public:
    MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache);

private:
    void Update() final;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;

    uint32_t m_current_icon_hash {kInvalidIconHash};
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    lv_obj_t* m_soc_label {nullptr};

    // Maybe TMP
    std::unique_ptr<uint8_t[]> m_static_map_buffer;
    std::unique_ptr<Image> m_static_map_image;
};
