#include "map_screen.hh"

#include "painter.hh"

#include <radbuzz_font_22.h>

MapScreen::MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache)
    : ScreenBase(parent)
    , m_image_cache(image_cache)
    , m_tile_cache(tile_cache)
{
    m_static_map_buffer = std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(
        aligned_alloc(64, hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t))));
    m_static_map_image = std::make_unique<Image>(
        std::span<const uint8_t> {m_static_map_buffer.get(),
                                  hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t)},
        hal::kDisplayWidth,
        hal::kDisplayHeight);

    m_screen = lv_image_create(nullptr);

    lv_image_set_src(m_screen, &m_static_map_image->lv_image_dsc);

    m_current_icon = lv_image_create(m_screen);
    lv_obj_center(m_current_icon);
    lv_image_set_src(m_current_icon, &m_image_cache.Lookup(kInvalidIconHash)->GetDsc());
    lv_obj_align(m_current_icon, LV_ALIGN_CENTER, 0, 0);

    auto label_box = lv_obj_create(m_screen);
    lv_obj_set_size(label_box, 400, 100);
    lv_obj_align(label_box, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_obj_set_style_border_width(label_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label_box, LV_OPA_TRANSP, LV_PART_MAIN);

    m_description_label = lv_label_create(label_box);
    lv_obj_align(m_description_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(m_description_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_label_set_long_mode(m_description_label, LV_LABEL_LONG_WRAP);

    m_distance_left_label = lv_label_create(label_box);
    lv_obj_align(m_distance_left_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(m_distance_left_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_label_set_long_mode(m_distance_left_label, LV_LABEL_LONG_WRAP);
}

void
MapScreen::Update()
{
    auto state = m_parent.m_state.CheckoutReadonly();
    auto state_hash = state.Get<AS::current_icon_hash>();

    if (m_current_icon_hash != state_hash)
    {
        if (auto image = m_image_cache.Lookup(state_hash); image)
        {
            m_current_icon_hash = state_hash;
            lv_image_set_src(m_current_icon, &image->GetDsc());
        }
    }

    auto gps_data = state.Get<AS::position>();

    auto t = ToTile(gps_data->pixel_position);

    // Calculate the center of the display
    int display_cx = hal::kDisplayWidth / 2;
    int display_cy = hal::kDisplayHeight / 2;

    // Calculate the top-left pixel in OSM coordinates that should be at (0,0) on the display
    int start_x = gps_data->pixel_position.x - display_cx;
    int start_y = gps_data->pixel_position.y - display_cy;

    // Calculate how many tiles are needed to cover the display
    constexpr int num_tiles_x = (hal::kDisplayWidth + kTileSize - 1) / kTileSize + 1;
    constexpr int num_tiles_y = (hal::kDisplayHeight + kTileSize - 1) / kTileSize + 1;

    // For each tile, calculate its top-left position in display coordinates and blit it
    constexpr auto kMaxTiles = num_tiles_x * num_tiles_y;

    for (int y = 0; y < num_tiles_y; ++y)
    {
        for (int x = 0; x < num_tiles_x; ++x)
        {
            int tile_x = (start_x / kTileSize) + x;
            int tile_y = (start_y / kTileSize) + y;

            int tile_pixel_x = tile_x * kTileSize;
            int tile_pixel_y = tile_y * kTileSize;

            int16_t dst_x = static_cast<int16_t>(tile_pixel_x - start_x);
            int16_t dst_y = static_cast<int16_t>(tile_pixel_y - start_y);

            auto tile = m_tile_cache.GetTile(ToTile(Point {tile_pixel_x, tile_pixel_y}));
            painter::Blit(reinterpret_cast<uint16_t*>(m_static_map_buffer.get()),
                          tile,
                          painter::Rect {dst_x, dst_y});
        }
    }

    lv_label_set_text(m_description_label,
                      std::format("{}", *state.Get<AS::next_street>()).c_str());
    lv_label_set_text(m_distance_left_label,
                      std::format("{} m", state.Get<AS::distance_to_next>()).c_str());

    // TMP!
    lv_label_set_text(m_description_label,
                      std::format("Controller: {}°C, Battery: {:.1f}V",
                                  state.Get<AS::controller_temperature>(),
                                  static_cast<float>(state.Get<AS::battery_millivolts>()) / 1000.0f)
                          .c_str());
}
