#include "map_screen.hh"
#include "painter.hh"

#include <algorithm>
#include <array>
#include <radbuzz_font_22.h>

MapScreen::MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache)
    : ScreenBase(parent, lv_image_create(nullptr))
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

    lv_image_set_src(m_screen, &m_static_map_image->lv_image_dsc);

    m_current_icon = lv_image_create(m_screen);
    lv_obj_center(m_current_icon);
    lv_image_set_src(m_current_icon, &m_image_cache.Lookup(kInvalidIconHash)->GetDsc());
    lv_obj_align(m_current_icon, LV_ALIGN_CENTER, 0, 0);

    m_soc_label = lv_label_create(m_screen);
    lv_obj_align(m_soc_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(m_soc_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_soc_label, LV_OPA_TRANSP, LV_PART_MAIN);


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
    auto ro = m_parent.m_state.CheckoutReadonly();
    auto state_hash = ro.Get<AS::current_icon_hash>();

    if (m_current_icon_hash != state_hash)
    {
        if (auto image = m_image_cache.Lookup(state_hash); image)
        {
            m_current_icon_hash = state_hash;
            lv_image_set_src(m_current_icon, &image->GetDsc());
        }
    }

    auto pixel_position = *ro.Get<AS::pixel_position>();

    auto t = ToTile(pixel_position);

    // Calculate the center of the display
    int display_cx = hal::kDisplayWidth / 2;
    int display_cy = hal::kDisplayHeight / 2;

    // Calculate the top-left pixel in OSM coordinates that should be at (0,0) on the display
    int start_x = pixel_position.x - display_cx;
    int start_y = pixel_position.y - display_cy;

    // For each tile, calculate its top-left position in display coordinates and blit it
    m_blit_ops.clear();
    auto dst_data = reinterpret_cast<uint16_t*>(m_static_map_buffer.get());
    for (int y = 0; y < kNumTilesY; ++y)
    {
        for (int x = 0; x < kNumTilesX; ++x)
        {
            constexpr auto kSoftwareBlitLimit = 2048;

            int tile_x = (start_x / kTileSize) + x;
            int tile_y = (start_y / kTileSize) + y;

            int tile_pixel_x = tile_x * kTileSize;
            int tile_pixel_y = tile_y * kTileSize;

            auto dst_x = static_cast<int16_t>(tile_pixel_x - start_x);
            auto dst_y = static_cast<int16_t>(tile_pixel_y - start_y);

            auto tile =
                m_tile_cache.GetTile(ToTile(Point {tile_pixel_x, tile_pixel_y, kDefaultZoom}));

            int32_t src_offset_x = 0;
            int32_t src_offset_y = 0;
            int32_t dst_offset_x = dst_x;
            int32_t dst_offset_y = dst_y;
            auto clipped_width = static_cast<int32_t>(tile.Width());
            auto clipped_height = static_cast<int32_t>(tile.Height());

            // Clip the tile against the visible screen area and shift the source region accordingly.
            if (dst_offset_x < 0)
            {
                src_offset_x = -dst_offset_x;
                clipped_width += dst_offset_x;
                dst_offset_x = 0;
            }
            if (dst_offset_y < 0)
            {
                src_offset_y = -dst_offset_y;
                clipped_height += dst_offset_y;
                dst_offset_y = 0;
            }

            clipped_width =
                std::min(clipped_width, static_cast<int32_t>(hal::kDisplayWidth) - dst_offset_x);
            clipped_height =
                std::min(clipped_height, static_cast<int32_t>(hal::kDisplayHeight) - dst_offset_y);

            if (clipped_width <= 0 || clipped_height <= 0)
            {
                continue;
            }

            if (clipped_width * clipped_height < kSoftwareBlitLimit)
            {
                painter::Blit(dst_data, tile, {dst_offset_x, dst_offset_y, clipped_width, clipped_height});
            }
            else
            {
                m_blit_ops.push_back(hal::BlitOperation {
                    .src_data = tile.Data16().data(),
                    .dst_data = dst_data,
                    .src_width = static_cast<int16_t>(tile.Width()),
                    .src_height = static_cast<int16_t>(tile.Height()),
                    .src_offset_x = static_cast<int16_t>(src_offset_x),
                    .src_offset_y = static_cast<int16_t>(src_offset_y),
                    .dst_offset_x = static_cast<int16_t>(dst_offset_x),
                    .dst_offset_y = static_cast<int16_t>(dst_offset_y),
                    .width = static_cast<int16_t>(clipped_width),
                    .height = static_cast<int16_t>(clipped_height),
                    .rotation = hal::Rotation::k0,
                });
            }
        }
    }

    m_parent.m_blitter.BlitOperations(
        std::span<const hal::BlitOperation> {m_blit_ops.data(), m_blit_ops.size()});

    lv_label_set_text(m_soc_label, std::format("{}%", ro.Get<AS::battery_soc>()).c_str());
    lv_label_set_text(m_description_label, std::format("{}", *ro.Get<AS::next_street>()).c_str());
    lv_label_set_text(m_distance_left_label,
                      std::format("{} m", ro.Get<AS::distance_to_next>()).c_str());

    // TMP!
    lv_label_set_text(m_description_label,
                      std::format("Controller: {}°C, Battery: {:.1f}V",
                                  ro.Get<AS::controller_temperature>(),
                                  static_cast<float>(ro.Get<AS::battery_millivolts>()) / 1000.0f)
                          .c_str());
}

void
MapScreen::HandleInput(hal::IInput::EventType event)
{
    switch (event)
    {
    case hal::IInput::EventType::kLeft:
        [[fallthrough]]; // For now
    case hal::IInput::EventType::kRight:
        m_parent.ActivateScreen(*m_parent.m_trip_meter_screen);
        break;
    case hal::IInput::EventType::kButtonDown:
        m_parent.ActivateScreen(*m_parent.m_settings_menu_screen);
        break;
    default:
        break;
    }
}
