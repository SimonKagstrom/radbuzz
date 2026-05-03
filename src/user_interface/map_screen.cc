#include "map_screen.hh"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <radbuzz_font_16.h>
#include <radbuzz_font_22.h>
#include <radbuzz_font_60.h>

namespace
{
void
DrawBatteryIndicator(BlankAlphaImage& battery_indicator, uint8_t soc)
{
    constexpr int kBorder = 3;

    const int w = static_cast<int>(battery_indicator.Width());
    const int h = static_cast<int>(battery_indicator.Height());

    auto* battery_canvas = lv_canvas_create(nullptr);
    lv_canvas_set_buffer(battery_canvas,
                         static_cast<void*>(battery_indicator.WritableData16()),
                         battery_indicator.Width(),
                         battery_indicator.Height(),
                         LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(battery_canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t battery_layer;
    lv_canvas_init_layer(battery_canvas, &battery_layer);

    lv_draw_rect_dsc_t outline_dsc;
    lv_draw_rect_dsc_init(&outline_dsc);
    outline_dsc.bg_color = lv_color_white();
    outline_dsc.bg_opa = LV_OPA_COVER;
    outline_dsc.border_color = lv_color_white(); // lv_palette_main(LV_PALETTE_GREY);
    outline_dsc.border_opa = LV_OPA_COVER;
    outline_dsc.border_width = kBorder;
    outline_dsc.radius = 0;

    const lv_area_t body_area {.x1 = 0, .y1 = 0, .x2 = w - 1, .y2 = h - 1};
    lv_draw_rect(&battery_layer, &outline_dsc, &body_area);

    const int inner_x1 = kBorder;
    const int inner_y1 = kBorder;
    const int inner_x2 = w - kBorder - 1;
    const int inner_y2 = h - kBorder - 1;
    const int inner_h = std::max(0, inner_y2 - inner_y1 + 1);

    const uint8_t soc_clamped = std::min<uint8_t>(soc, 100);
    const int fill_h = (inner_h * static_cast<int>(soc_clamped) + 99) / 100;

    auto color = LV_PALETTE_GREEN;

    if (soc_clamped < 20)
    {
        color = LV_PALETTE_RED;
    }
    else if (soc_clamped < 50)
    {
        color = LV_PALETTE_ORANGE;
    }

    if (fill_h > 0)
    {
        lv_draw_rect_dsc_t fill_dsc;
        lv_draw_rect_dsc_init(&fill_dsc);
        fill_dsc.bg_color = lv_palette_main(color);
        fill_dsc.bg_opa = LV_OPA_COVER;
        fill_dsc.border_width = 0;
        fill_dsc.radius = 0;

        const lv_area_t fill_area {
            .x1 = inner_x1,
            .y1 = inner_y2 - fill_h + 1,
            .x2 = inner_x2,
            .y2 = inner_y2,
        };
        lv_draw_rect(&battery_layer, &fill_dsc, &fill_area);
    }

    if (soc_clamped < 30)
    {
        char soc_text[5] = {};
        std::snprintf(soc_text, sizeof(soc_text), "%u", static_cast<unsigned>(soc_clamped));

        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.text = soc_text;
        label_dsc.color = lv_color_black();
        label_dsc.font = &radbuzz_font_16;
        label_dsc.align = LV_TEXT_ALIGN_LEFT;

        const lv_area_t text_area {
            .x1 = 2,
            .y1 = 2,
            .x2 = w - 3,
            .y2 = h - 3,
        };
        lv_draw_label(&battery_layer, &label_dsc, &text_area);
    }

    lv_canvas_finish_layer(battery_canvas, &battery_layer);
    lv_obj_delete(battery_canvas);
}
} // namespace

void
MapScreen::DrawRangeCircle(lv_layer_t* layer)
{
    if (m_zoom != kLandscapeZoom)
    {
        return;
    }

    const int center_x =
        lv_obj_get_x(m_position_dot_obj) + static_cast<int>(m_position_dot.Width()) / 2;
    const int center_y =
        lv_obj_get_y(m_position_dot_obj) + static_cast<int>(m_position_dot.Height()) / 2;

    auto ro = m_parent.m_state.CheckoutReadonly();
    const auto conf = ro.Get<AS::configuration>();
    const uint8_t battery_soc = std::min<uint8_t>(ro.Get<AS::battery_soc>(), 100);
    const uint8_t wh_per_km = std::max<uint8_t>(1, conf->wh_per_km_for_range_estimation);
    const auto vehicle_point = OsmPointToPoint(*ro.Get<AS::pixel_position>(), m_zoom);

    // Estimate Wh left from configured pack size and SoC to avoid noisy voltage-based range.
    constexpr float kNominalCellVoltageV = 3.7f;
    const float pack_nominal_voltage_v =
        static_cast<float>(conf->battery_cell_series) * kNominalCellVoltageV;
    const float full_pack_wh = static_cast<float>(conf->battery_amp_hours) * pack_nominal_voltage_v;
    const float wh_left = full_pack_wh * (static_cast<float>(battery_soc) / 100.0f);
    const float estimated_range_km = wh_left / static_cast<float>(wh_per_km);

    constexpr int kMinRangeCircleRadiusPx = 24;
    const float meters_per_pixel = MetersPerPixelAtPoint(vehicle_point);
    const float radius_px_float = (estimated_range_km * 1000.0f) / meters_per_pixel;
    const int radius = std::clamp(static_cast<int>(radius_px_float),
                                  kMinRangeCircleRadiusPx,
                                  static_cast<int>(std::numeric_limits<uint16_t>::max()));

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_black();
    arc_dsc.width = 3;
    arc_dsc.opa = LV_OPA_COVER;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    arc_dsc.center = lv_point_t {
        .x = static_cast<int16_t>(center_x),
        .y = static_cast<int16_t>(center_y),
    };
    arc_dsc.radius = static_cast<uint16_t>(radius);
    arc_dsc.rounded = 1;
    lv_draw_arc(layer, &arc_dsc);
}

MapScreen::MapScreen(UserInterface& parent,
                     ImageCache& image_cache,
                     TileCache& tile_cache,
                     uint8_t zoom)
    : ScreenBase(parent, lv_obj_create(nullptr))
    , m_image_cache(image_cache)
    , m_tile_cache(tile_cache)
    , m_zoom(zoom)
    , m_touch_timer(m_parent.StartTimer(0ms))
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
            auto* layer = lv_event_get_layer(e);
            auto* dst_data = static_cast<uint16_t*>(static_cast<void*>(layer->draw_buf->data));
            for (auto& op : self->m_blit_ops)
            {
                op.dst_data = dst_data;
            }
            self->m_parent.m_blitter.BlitOperations(std::span<const hal::BlitOperation> {
                self->m_blit_ops.data(), self->m_blit_ops.size()});
            self->DrawRangeCircle(layer);
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

    auto* speed_triangle_canvas = lv_canvas_create(nullptr);
    lv_canvas_set_buffer(speed_triangle_canvas,
                         static_cast<void*>(m_speed_triangle.WritableData16()),
                         m_speed_triangle.Width(),
                         m_speed_triangle.Height(),
                         LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(speed_triangle_canvas, lv_color_black(), LV_OPA_TRANSP);

    lv_layer_t speed_layer;
    lv_canvas_init_layer(speed_triangle_canvas, &speed_layer);

    lv_draw_rect_dsc_t speed_panel_dsc;
    lv_draw_rect_dsc_init(&speed_panel_dsc);
    speed_panel_dsc.bg_color = lv_color_black();
    speed_panel_dsc.bg_opa = LV_OPA_COVER;
    speed_panel_dsc.border_width = 0;
    speed_panel_dsc.radius = 24;

    const int speed_w = static_cast<int>(m_speed_triangle.Width());
    const int speed_h = static_cast<int>(m_speed_triangle.Height());

    const lv_area_t rounded_panel {
        .x1 = 0,
        .y1 = 0,
        .x2 = speed_w - 1,
        .y2 = speed_h - 1,
    };
    lv_draw_rect(&speed_layer, &speed_panel_dsc, &rounded_panel);

    // Restore square corners for all but lower-right.
    const int r = speed_panel_dsc.radius;
    const lv_area_t top_left_corner {.x1 = 0, .y1 = 0, .x2 = r - 1, .y2 = r - 1};
    const lv_area_t top_right_corner {.x1 = speed_w - r, .y1 = 0, .x2 = speed_w - 1, .y2 = r - 1};
    const lv_area_t bottom_left_corner {.x1 = 0, .y1 = speed_h - r, .x2 = r - 1, .y2 = speed_h - 1};

    speed_panel_dsc.radius = 0;
    lv_draw_rect(&speed_layer, &speed_panel_dsc, &top_left_corner);
    lv_draw_rect(&speed_layer, &speed_panel_dsc, &top_right_corner);
    lv_draw_rect(&speed_layer, &speed_panel_dsc, &bottom_left_corner);

    lv_canvas_finish_layer(speed_triangle_canvas, &speed_layer);
    lv_obj_delete(speed_triangle_canvas);

    m_speed_triangle_obj = lv_image_create(m_screen);
    lv_image_set_src(m_speed_triangle_obj, &m_speed_triangle.lv_image_dsc);
    lv_obj_set_align(m_speed_triangle_obj, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(m_speed_triangle_obj, 0, 0);

    m_speed_digits_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_speed_digits_label, &radbuzz_font_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speed_digits_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_speed_digits_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_align(m_speed_digits_label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(m_speed_digits_label, 18, 12);
    lv_label_set_text(m_speed_digits_label, "0");

    const uint8_t battery_soc =
        std::min<uint8_t>(m_parent.m_state.CheckoutReadonly().Get<AS::battery_soc>(), 100);
    DrawBatteryIndicator(m_battery_indicator, battery_soc);
    m_last_battery_soc = battery_soc;

    m_battery_indicator_obj = lv_image_create(m_screen);
    lv_image_set_src(m_battery_indicator_obj, &m_battery_indicator.lv_image_dsc);
    lv_obj_set_align(m_battery_indicator_obj, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(m_battery_indicator_obj,
                   hal::kDisplayWidth - static_cast<int>(m_battery_indicator.Width()) - 4,
                   4);


    m_current_view_center = *m_parent.m_state.CheckoutReadonly().Get<AS::pixel_position>();
}

void
MapScreen::SetZoom(uint8_t zoom)
{
    m_zoom = zoom;
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

    auto pixel_position = OsmPointToPoint(*ro.Get<AS::pixel_position>(), m_zoom);

    if (m_touch_timer->IsExpired())
    {
        m_current_view_center = pixel_position;
    }

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

            auto tile = m_tile_cache.GetTile(ToTile(Point {tile_pixel_x, tile_pixel_y, m_zoom}));

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
    lv_label_set_text(m_speed_digits_label, std::format("{}", ro.Get<AS::speed>()).c_str());

    const uint8_t battery_soc = std::min<uint8_t>(ro.Get<AS::battery_soc>(), 100);
    if (battery_soc != m_last_battery_soc)
    {
        DrawBatteryIndicator(m_battery_indicator, battery_soc);
        m_last_battery_soc = battery_soc;
        lv_obj_invalidate(m_battery_indicator_obj);
    }

    // TMP!
    lv_label_set_text(m_description_label,
                      std::format("Controller: {}°C, Battery: {:.1f}V",
                                  ro.Get<AS::controller_temperature>(),
                                  static_cast<float>(ro.Get<AS::battery_millivolts>()) / 1000.0f)
                          .c_str());
}

void
MapScreen::HandleInput(const Input::Event& event)
{
    switch (event.type)
    {
    case hal::IInput::EventType::kLeft:
        // Ugly
        if (m_zoom == kDefaultZoom)
        {
            SetZoom(kCityZoom);
        }
        else if (m_zoom == kCityZoom)
        {
            SetZoom(kLandscapeZoom);
        }
        else
        {
            m_parent.ActivateScreen(*m_parent.m_trip_meter_screen);
        }
        break;
    case hal::IInput::EventType::kRight:
        if (m_zoom == kLandscapeZoom)
        {
            SetZoom(kCityZoom);
        }
        else if (m_zoom == kCityZoom)
        {
            SetZoom(kDefaultZoom);
        }
        else
        {
            m_parent.ActivateScreen(*m_parent.m_trip_meter_screen);
        }
        break;
    case hal::IInput::EventType::kButtonDown:
        m_parent.ActivateScreen(*m_parent.m_settings_menu_screen);
        break;

    case hal::IInput::EventType::kTouchDown: {
        m_touch_timer = m_parent.StartTimer(3s);
        m_last_touch_x = event.x;
        m_last_touch_y = event.y;
        break;
    }
    case hal::IInput::EventType::kTouchMove: {
        m_touch_timer = m_parent.StartTimer(3s);

        m_current_view_center.x -= event.x - m_last_touch_x;
        m_current_view_center.y -= event.y - m_last_touch_y;
        m_last_touch_x = event.x;
        m_last_touch_y = event.y;
    }
    break;

    default:
        break;
    }
}
