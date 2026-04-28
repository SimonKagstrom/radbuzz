#include "map_screen.hh"

#include <algorithm>
#include <radbuzz_font_22.h>

MapScreen::MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache)
    : ScreenBase(parent, lv_obj_create(nullptr))
    , m_image_cache(image_cache)
    , m_tile_cache(tile_cache)
{
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_TRANSP, 0);

    /*
     * Tiles are blitted directly into the LVGL render buffer during LV_EVENT_DRAW_MAIN,
     * eliminating the intermediate static_map_buffer and its associated software copy.
     */
    lv_obj_add_event_cb(
        m_screen,
        [](lv_event_t* e) {
            auto* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
            auto* dst_data =
                static_cast<uint16_t*>(static_cast<void*>(lv_event_get_layer(e)->draw_buf->data));
            for (auto& op : self->m_blit_ops)
            {
                op.dst_data = dst_data;
            }
            self->m_parent.m_blitter.BlitOperations(std::span<const hal::BlitOperation> {
                self->m_blit_ops.data(), self->m_blit_ops.size()});
        },
        LV_EVENT_DRAW_MAIN,
        this);

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

    auto* position_dot_canvas = lv_canvas_create(nullptr);
    lv_canvas_set_buffer(position_dot_canvas,
                         static_cast<void*>(m_position_dot.WritableData16()),
                         m_position_dot.Width(),
                         m_position_dot.Height(),
                         LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(position_dot_canvas, lv_color_black(), LV_OPA_TRANSP);

    lv_layer_t layer;
    lv_canvas_init_layer(position_dot_canvas, &layer);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_palette_main(LV_PALETTE_ORANGE);
    rect_dsc.bg_opa = LV_OPA_60;
    rect_dsc.border_width = 0;
    rect_dsc.radius = LV_RADIUS_CIRCLE;

    constexpr int radius = 16;
    constexpr int diameter = radius * 2;
    const int x1 = (static_cast<int>(m_position_dot.Width()) - diameter) / 2;
    const int y1 = (static_cast<int>(m_position_dot.Height()) - diameter) / 2;
    const lv_area_t dot_area {.x1 = x1, .y1 = y1, .x2 = x1 + diameter - 1, .y2 = y1 + diameter - 1};

    lv_draw_rect(&layer, &rect_dsc, &dot_area);
    lv_canvas_finish_layer(position_dot_canvas, &layer);

    lv_obj_delete(position_dot_canvas);

    m_position_dot_obj = lv_image_create(m_screen);
    lv_image_set_src(m_position_dot_obj, &m_position_dot.lv_image_dsc);
    lv_obj_set_align(m_position_dot_obj, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(m_position_dot_obj,
                   hal::kDisplayWidth / 2 - static_cast<int>(m_position_dot.Width()) / 2,
                   hal::kDisplayHeight / 2 - static_cast<int>(m_position_dot.Height()) / 2);


    m_current_view_center = *m_parent.m_state.CheckoutReadonly().Get<AS::pixel_position>();
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

    // Keep the view centered on the bike for now.
    m_current_view_center = pixel_position;

    // Calculate the center of the display
    int display_cx = hal::kDisplayWidth / 2;
    int display_cy = hal::kDisplayHeight / 2;

    // Calculate the top-left pixel in OSM coordinates that should be at (0,0) on the display
    int start_x = m_current_view_center.x - display_cx;
    int start_y = m_current_view_center.y - display_cy;

    const int dot_center_x = display_cx + (pixel_position.x - m_current_view_center.x);
    const int dot_center_y = display_cy + (pixel_position.y - m_current_view_center.y);
    lv_obj_set_pos(m_position_dot_obj,
                   dot_center_x - static_cast<int>(m_position_dot.Width()) / 2,
                   dot_center_y - static_cast<int>(m_position_dot.Height()) / 2);

    // Build blit ops; dst_data is filled in by the LV_EVENT_DRAW_MAIN callback at render time.
    m_blit_ops.clear();

    for (int y = 0; y < kNumTilesY; ++y)
    {
        for (int x = 0; x < kNumTilesX; ++x)
        {
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

            m_blit_ops.push_back(hal::BlitOperation {
                .src_data = tile.Data16().data(),
                .dst_data = nullptr,
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

    lv_obj_invalidate(m_screen);

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
MapScreen::HandleInput(Input::Event event)
{
    switch (event.type)
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
