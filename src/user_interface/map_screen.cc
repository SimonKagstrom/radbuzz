#include "map_screen.hh"

#include "bresenham.hh"
#include "cohen_sutherland.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numbers>
#include <radbuzz_font_16.h>
#include <radbuzz_font_22.h>
#include <radbuzz_font_60.h>
#include <radbuzz_symbols_40.h>


void
MapScreen::DrawRangeCircle(lv_layer_t* layer, RangeCircleType type)
{
    constexpr auto kDisplayCenterX = hal::kDisplayWidth / 2;
    constexpr auto kDisplayCenterY = hal::kDisplayHeight / 2;

    auto ro = m_parent.m_state.CheckoutReadonly();
    const auto conf = ro.Get<AS::configuration>();
    const uint8_t battery_soc = std::min<uint8_t>(ro.Get<AS::battery_soc>(), 100);
    const uint8_t wh_per_km = std::max<uint8_t>(1, conf->wh_per_km_for_range_estimation);
    const auto vehicle_point = OsmPointToPoint(m_current_range_circle_center, m_zoom);
    const int center_x = kDisplayCenterX + (vehicle_point.x - m_current_view_center.x);
    const int center_y = kDisplayCenterY + (vehicle_point.y - m_current_view_center.y);

    // Estimate Wh left from configured pack size and SoC to avoid noisy voltage-based range.
    constexpr float kNominalCellVoltageV = 3.7f;
    const float pack_nominal_voltage_v =
        static_cast<float>(conf->battery_cell_series) * kNominalCellVoltageV;
    const float full_pack_wh = static_cast<float>(conf->battery_amp_hours) * pack_nominal_voltage_v;
    const float wh_left = full_pack_wh * (static_cast<float>(battery_soc) / 100.0f);
    // Round trip circle is smaller since it represents the distance to the furthest point and back
    const float estimated_range_km = wh_left / static_cast<float>(wh_per_km) /
                                     (type == RangeCircleType::kRoundTrip ? 2.0f : 1.0f);

    constexpr int kMinRangeCircleRadiusPx = 24;
    const float meters_per_pixel = MetersPerPixelAtPoint(vehicle_point);
    const float radius_px_float = (estimated_range_km * 1000.0f) / meters_per_pixel;
    const int radius = std::clamp(static_cast<int>(radius_px_float),
                                  kMinRangeCircleRadiusPx,
                                  static_cast<int>(std::numeric_limits<uint16_t>::max()));

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_black();
    arc_dsc.width = type == RangeCircleType::kFurthest ? 3 : 2;
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

void
MapScreen::DrawTripLines(lv_layer_t* layer)
{
    auto p = m_parent.m_trip_computer.GetDisplayLog();
    auto& log = p.second;

    if (log.size() < 2)
    {
        return;
    }

    auto it = log.begin();
    auto last = it;
    constexpr int kDisplayCenterX = hal::kDisplayWidth / 2;
    constexpr int kDisplayCenterY = hal::kDisplayHeight / 2;

    const auto kLowPowerColor = lv_color_to_u16(lv_palette_main(LV_PALETTE_GREEN));
    const auto kMidPowerColor = lv_color_to_u16(lv_palette_main(LV_PALETTE_AMBER));
    const auto kHighPowerColor = lv_color_to_u16(lv_palette_main(LV_PALETTE_RED));

    const auto kMaxPower = m_parent.m_state.CheckoutReadonly().Get<AS::configuration>()->max_watts;

    auto* dst = static_cast<uint16_t*>(static_cast<void*>(layer->draw_buf->data));
    for (++it; it != log.end(); ++it, ++last)
    {
        auto last_position = OsmPointToPoint(last->position, m_zoom);
        auto current_position = OsmPointToPoint(it->position, m_zoom);

        // Transform from map-world coordinates to display coordinates.
        last_position.x = (last_position.x - m_current_view_center.x) + kDisplayCenterX;
        last_position.y = (last_position.y - m_current_view_center.y) + kDisplayCenterY;
        current_position.x = (current_position.x - m_current_view_center.x) + kDisplayCenterX;
        current_position.y = (current_position.y - m_current_view_center.y) + kDisplayCenterY;

        if (!cs::ClipLineToDisplay(
                last_position.x, last_position.y, current_position.x, current_position.y))
        {
            continue;
        }

        auto color = kHighPowerColor;
        if (it->power < kMaxPower / 3)
        {
            color = kLowPowerColor;
        }
        else if (it->power < 2 * kMaxPower / 3)
        {
            color = kMidPowerColor;
        }

        painter::DrawClippedLine<Point>(dst,
                                        {last_position.x, last_position.y},
                                        {current_position.x, current_position.y},
                                        5,
                                        color);
    }
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
    lv_obj_set_scrollbar_mode(m_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);

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

            if (self->m_rotation_enabled == false)
            {
                for (auto& op : self->m_blit_ops)
                {
                    op.dst_data = dst_data;
                }
                self->m_parent.m_blitter.BlitOperations(std::span<const hal::BlitOperation> {
                    self->m_blit_ops.data(), self->m_blit_ops.size()});
                self->m_parent.m_blitter.WaitForBlitsDone();

                // Only for the most zoomed out map, because of our insane range
                if (self->m_zoom == kLandscapeZoom)
                {
                    self->DrawRangeCircle(layer, RangeCircleType::kFurthest);
                    self->DrawRangeCircle(layer, RangeCircleType::kRoundTrip);
                }
                self->DrawTripLines(layer);
            }
            else
            {
                self->m_parent.m_blitter.WaitForBlitsDone();
                self->RotateBackground(self->m_rotation, dst_data);
            }
        },
        LV_EVENT_DRAW_MAIN,
        this);


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


    m_soc_label = lv_label_create(m_screen);
    lv_obj_align(m_soc_label, LV_ALIGN_TOP_RIGHT, -5, 0);
    lv_obj_set_style_text_font(m_soc_label, &radbuzz_symbols_40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_soc_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_soc_label, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);


    m_gps_lost = lv_label_create(m_screen);
    lv_obj_align_to(m_gps_lost, m_soc_label, LV_ALIGN_OUT_LEFT_TOP, 40, 0);
    lv_obj_set_style_text_font(m_gps_lost, &radbuzz_symbols_40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_gps_lost, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_gps_lost, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_label_set_text(m_gps_lost, LV_SYMBOL_GPS);

    m_wifi_active = lv_label_create(m_screen);
    lv_obj_align_to(m_wifi_active, m_gps_lost, LV_ALIGN_OUT_LEFT_TOP, -10, 0);
    lv_obj_set_style_text_font(m_wifi_active, &radbuzz_symbols_40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_wifi_active, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_wifi_active, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_label_set_text(m_wifi_active, LV_SYMBOL_WIFI);

    m_parked = lv_label_create(m_screen);
    lv_obj_align_to(m_parked, m_wifi_active, LV_ALIGN_OUT_LEFT_TOP, -10, 0);
    lv_obj_set_style_text_font(m_parked, &radbuzz_symbols_40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_parked, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_parked, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_label_set_text(m_parked, "P");

    // Power = regen bar
    m_power_bar = lv_obj_create(m_screen);
    lv_obj_set_size(m_power_bar, 10, 60);
    lv_obj_align(m_power_bar, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_border_width(m_power_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_power_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_power_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_power_bar, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_power_bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);

    // Left pane
    auto left_box = lv_obj_create(m_screen);
    lv_obj_set_size(left_box, 128 + 10, hal::kDisplayHeight + 10);
    lv_obj_align(left_box, LV_ALIGN_TOP_LEFT, -10, -10);
    lv_obj_set_style_bg_opa(left_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(left_box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(left_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(left_box, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(left_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(left_box, LV_OBJ_FLAG_SCROLLABLE);

    // Push left rounded corners off-screen for the navigation pane while keeping right corners.
    constexpr int kLeftCornerClipPx = 16;
    constexpr int kPaneCornerRadius = 18;

    // Digital speedometer
    m_speedometer_box = lv_obj_create(left_box);
    lv_obj_set_size(m_speedometer_box, 128 + kLeftCornerClipPx, 128);
    lv_obj_align(m_speedometer_box, LV_ALIGN_TOP_LEFT, -32, -32);
    lv_obj_set_style_border_width(m_speedometer_box, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(m_speedometer_box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_speedometer_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_speedometer_box, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_speedometer_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(m_speedometer_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(m_speedometer_box, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_opa(m_speedometer_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(m_speedometer_box, kPaneCornerRadius, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(m_speedometer_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_speedometer_box, LV_OBJ_FLAG_SCROLLABLE);

    m_speed_digits_label = lv_label_create(m_speedometer_box);
    lv_obj_set_style_text_font(m_speed_digits_label, &radbuzz_font_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speed_digits_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_speed_digits_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_align(m_speed_digits_label, LV_ALIGN_CENTER);
    lv_obj_set_pos(m_speed_digits_label, 8, 4);
    lv_label_set_text(m_speed_digits_label, "0");
    // End of digital speedometer setup

    // Navigation
    m_navigation_box = lv_obj_create(left_box);
    lv_obj_set_size(m_navigation_box, 128 + kLeftCornerClipPx, 128);
    lv_obj_align(m_navigation_box, LV_ALIGN_BOTTOM_LEFT, -32, 32);
    lv_obj_set_style_border_width(m_navigation_box, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(m_navigation_box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_navigation_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_navigation_box, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_navigation_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(m_navigation_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(m_navigation_box, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_opa(m_navigation_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(m_navigation_box, kPaneCornerRadius, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(m_navigation_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_navigation_box, LV_OBJ_FLAG_SCROLLABLE);

    m_current_icon = lv_image_create(m_navigation_box);
    lv_obj_center(m_current_icon);
    lv_obj_set_style_img_recolor(m_current_icon, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(m_current_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_image_set_src(m_current_icon, &m_image_cache.Lookup(kInvalidIconHash)->GetDsc());
    lv_obj_align(m_current_icon, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(m_current_icon, LV_OBJ_FLAG_SCROLLABLE);

    m_distance_left_label = lv_label_create(m_navigation_box);
    lv_obj_align(m_distance_left_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(m_distance_left_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_label_set_long_mode(m_distance_left_label, LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(m_distance_left_label, LV_OBJ_FLAG_SCROLLABLE);
    // ... to here

    m_navigation_description_box = lv_obj_create(m_screen);
    lv_obj_align(m_navigation_description_box, LV_ALIGN_BOTTOM_LEFT, 128 - 32, 0);
    lv_obj_set_size(
        m_navigation_description_box, hal::kDisplayWidth - lv_obj_get_width(m_navigation_box), 32);
    lv_obj_set_style_border_width(m_navigation_description_box, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(m_navigation_description_box, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(m_navigation_description_box, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_navigation_description_box, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_navigation_description_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(m_navigation_description_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(m_navigation_description_box, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_opa(m_navigation_description_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(m_navigation_description_box, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(m_navigation_description_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(m_navigation_description_box, LV_OBJ_FLAG_SCROLLABLE);

    m_description_label = lv_label_create(m_navigation_description_box);
    lv_obj_set_style_text_font(m_description_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_align(m_description_label, LV_ALIGN_TOP_LEFT, 4, -20);
    lv_label_set_long_mode(m_description_label, LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(m_description_label, LV_OBJ_FLAG_SCROLLABLE);

    const uint8_t battery_soc =
        std::min<uint8_t>(m_parent.m_state.CheckoutReadonly().Get<AS::battery_soc>(), 100);

    SetZoom(m_zoom);
}

void
MapScreen::SetZoom(uint8_t zoom)
{
    m_zoom = zoom;

    auto pixel_position = m_parent.m_state.CheckoutReadonly().Get<AS::pixel_position>();

    // Setup the view center according to the zoom when changing
    m_current_view_center = OsmPointToPoint(*pixel_position, m_zoom);
    // And store the center of the range circle
    m_current_range_circle_center = m_current_view_center;
}

void
MapScreen::RotateBackground(int32_t angle_deg, uint16_t* dst)
{
    const float angle_rad = static_cast<float>(angle_deg) * std::numbers::pi_v<float> / 180.0f;
    const float cos_a = std::cosf(angle_rad);
    const float sin_a = std::sinf(angle_rad);
    // Rotate around a configurable display pivot (follow anchor in follow mode).
    const int cx = m_rotation_pivot_x;
    const int cy = m_rotation_pivot_y;
    // Always rotate around the loaded view center in source space.
    const int scx = kBgSize / 2;
    const int scy = kBgSize / 2;
    const uint16_t* src = m_background.WritableData16();
    for (int dy = 0; dy < hal::kDisplayHeight; ++dy)
    {
        const float fy = static_cast<float>(dy - cy);
        for (int dx = 0; dx < hal::kDisplayWidth; ++dx)
        {
            const float fx = static_cast<float>(dx - cx);
            const int sx = static_cast<int>(cos_a * fx + sin_a * fy) + scx;
            const int sy = static_cast<int>(-sin_a * fx + cos_a * fy) + scy;
            dst[dy * hal::kDisplayWidth + dx] = (sx >= 0 && sx < kBgSize && sy >= 0 && sy < kBgSize)
                                                    ? src[sy * kBgSize + sx]
                                                    : 0x0000;
        }
    }
}

void
MapScreen::Update()
{
    auto ro = m_parent.m_state.CheckoutReadonly();
    auto conf = ro.Get<AS::configuration>();

    auto pixel_position = OsmPointToPoint(*ro.Get<AS::pixel_position>(), m_zoom);

    constexpr int kFollowAnchorX = hal::kDisplayWidth / 2;
    constexpr int kFollowAnchorY = (hal::kDisplayHeight * 2) / 3;
    constexpr int kDisplayCenterX = hal::kDisplayWidth / 2;
    constexpr int kDisplayCenterY = hal::kDisplayHeight / 2;
    const bool follow_mode =
        m_touch_timer->IsExpired() && (m_zoom == kDefaultZoom) && conf->rotate_map;

    m_rotation_enabled = conf->rotate_map;

    m_rotation_pivot_x = kDisplayCenterX;
    m_rotation_pivot_y = kDisplayCenterY;
    m_rotation = 0;

    if (m_touch_timer->IsExpired() && m_zoom == kDefaultZoom)
    {
        m_current_view_center = pixel_position;
        if (follow_mode)
        {
            const float heading = ro.Get<AS::position>()->heading;
            const float normalized_rotation = std::fmod(heading + 180, 360.0f);
            m_rotation = static_cast<uint16_t>(std::lround(normalized_rotation));
            m_rotation_pivot_x = kFollowAnchorX;
            m_rotation_pivot_y = kFollowAnchorY;
        }
    }


    if (m_rotation_enabled)
    {
        BlitToRotationBuffer();
    }
    else
    {
        PrepareNonRotatedBlits();
    }

    // Calculate the center of the display
    int display_cx = kDisplayCenterX;
    int display_cy = kDisplayCenterY;

    const int dot_center_x =
        follow_mode ? kFollowAnchorX : (display_cx + (pixel_position.x - m_current_view_center.x));
    const int dot_center_y =
        follow_mode ? kFollowAnchorY : (display_cy + (pixel_position.y - m_current_view_center.y));
    lv_obj_set_pos(m_position_dot_obj,
                   dot_center_x - static_cast<int>(m_position_dot.Width()) / 2,
                   dot_center_y - static_cast<int>(m_position_dot.Height()) / 2);

    auto state_hash = ro.Get<AS::current_icon_hash>();
    auto navigation_active = ro.Get<AS::navigation_active>();

    lv_obj_set_flag(m_navigation_box, LV_OBJ_FLAG_HIDDEN, !navigation_active);
    lv_obj_set_flag(m_navigation_description_box, LV_OBJ_FLAG_HIDDEN, !navigation_active);

    lv_obj_set_flag(m_gps_lost, LV_OBJ_FLAG_HIDDEN, ro.Get<AS::gps_position_valid>());
    lv_obj_set_flag(m_wifi_active, LV_OBJ_FLAG_HIDDEN, !ro.Get<AS::wifi_connected>());
    lv_obj_set_flag(m_parked, LV_OBJ_FLAG_HIDDEN, ro.Get<AS::is_moving>());

    if (m_current_icon_hash != state_hash)
    {
        if (auto image = m_image_cache.Lookup(state_hash); image)
        {
            m_current_icon_hash = state_hash;
            lv_image_set_src(m_current_icon, &image->GetDsc());
        }
    }

    lv_label_set_text(m_description_label, std::format("{}", *ro.Get<AS::next_street>()).c_str());
    lv_label_set_text(m_distance_left_label,
                      std::format("{} m", ro.Get<AS::distance_to_next>()).c_str());
    lv_label_set_text(m_speed_digits_label, std::format("{}", ro.Get<AS::speed>()).c_str());

    const uint8_t battery_soc = std::min<uint8_t>(ro.Get<AS::battery_soc>(), 100);

    lv_obj_set_style_text_color(m_soc_label, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    if (battery_soc > 90)
    {
        lv_label_set_text(m_soc_label, LV_SYMBOL_BATTERY_FULL);
    }
    else if (battery_soc > 75)
    {
        lv_label_set_text(m_soc_label, LV_SYMBOL_BATTERY_3);
    }
    else if (battery_soc >= 40)
    {
        lv_label_set_text(m_soc_label, LV_SYMBOL_BATTERY_2);
    }
    else if (battery_soc > 20)
    {
        lv_obj_set_style_text_color(m_soc_label, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        lv_label_set_text(m_soc_label, LV_SYMBOL_BATTERY_1);
    }
    else
    {
        lv_obj_set_style_text_color(m_soc_label, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_label_set_text(m_soc_label, LV_SYMBOL_BATTERY_EMPTY);
    }

    if (conf->speedometer_type == SpeedometerType::kDigital ||
        conf->speedometer_type == SpeedometerType::kBoth)
    {
        lv_obj_remove_flag(m_speedometer_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m_speed_digits_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(m_speedometer_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_speed_digits_label, LV_OBJ_FLAG_HIDDEN);
    }


    constexpr int kPixelsAtMaxPower = 200;
    const auto watts_signed = static_cast<int16_t>(ro.Get<AS::current_power_w>());
    const int abs_watts = std::abs(watts_signed);
    const int max_watts = std::max(1, static_cast<int>(conf->max_watts));
    const int power_bar_size = (abs_watts * kPixelsAtMaxPower) / max_watts;
    const int clamped_power_bar_size = std::min(power_bar_size, kPixelsAtMaxPower);
    lv_obj_set_size(m_power_bar, 24, clamped_power_bar_size);
    const int power_bar_y_offset =
        watts_signed > 0 ? -(clamped_power_bar_size / 2) : (clamped_power_bar_size / 2);
    lv_obj_align(m_power_bar, LV_ALIGN_RIGHT_MID, -5, power_bar_y_offset);

    if (watts_signed > 0)
    {
        lv_obj_set_style_bg_color(m_power_bar, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    }
    else
    {
        lv_obj_set_style_bg_color(m_power_bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    }

    lv_obj_invalidate(m_screen);
}


void
MapScreen::PrepareNonRotatedBlits()
{
    constexpr auto kNumTilesX = (hal::kDisplayWidth + kTileSize - 1) / kTileSize + 1;
    constexpr auto kNumTilesY = (hal::kDisplayHeight + kTileSize - 1) / kTileSize + 1;

    // Calculate the center of the display
    int display_cx = hal::kDisplayWidth / 2;
    int display_cy = hal::kDisplayHeight / 2;

    // Calculate the top-left pixel in OSM coordinates that should be at (0,0) on the display
    int start_x = m_current_view_center.x - display_cx;
    int start_y = m_current_view_center.y - display_cy;

    // Build blit ops; dst_data is filled in by the LV_EVENT_DRAW_MAIN callback at render time.
    m_blit_ops.clear();

    for (int y = 0; y < kNumTilesY; ++y)
    {
        int tile_y = (start_y / kTileSize) + y;
        int tile_pixel_y = tile_y * kTileSize;
        auto dst_y = static_cast<int16_t>(tile_pixel_y - start_y);
        int32_t dst_offset_y = dst_y;

        int32_t src_offset_y = 0;
        int32_t clipped_height = kTileSize;

        if (dst_offset_y < 0)
        {
            src_offset_y = -dst_offset_y;
            clipped_height += dst_offset_y;
            dst_offset_y = 0;
        }

        clipped_height =
            std::min(clipped_height, static_cast<int32_t>(hal::kDisplayHeight) - dst_offset_y);

        if (clipped_height <= 0)
        {
            continue;
        }

        for (int x = 0; x < kNumTilesX; ++x)
        {
            int tile_x = (start_x / kTileSize) + x;
            int tile_pixel_x = tile_x * kTileSize;
            auto dst_x = static_cast<int16_t>(tile_pixel_x - start_x);

            int32_t src_offset_x = 0;
            int32_t dst_offset_x = dst_x;
            int32_t clipped_width = kTileSize;

            if (dst_offset_x < 0)
            {
                src_offset_x = -dst_offset_x;
                clipped_width += dst_offset_x;
                dst_offset_x = 0;
            }

            clipped_width =
                std::min(clipped_width, static_cast<int32_t>(hal::kDisplayWidth) - dst_offset_x);

            if (clipped_width <= 0)
            {
                continue;
            }

            auto tile = m_tile_cache.GetTile(ToTile(Point {tile_pixel_x, tile_pixel_y, m_zoom}));

            m_blit_ops.push_back(hal::BlitOperation {
                .src_data = tile.Data16().data(),
                .dst_data = nullptr,
                .src_width = static_cast<int16_t>(tile.Width()),
                .src_height = static_cast<int16_t>(tile.Height()),
                .src_stride = static_cast<int16_t>(tile.Width()),
                .src_offset_x = static_cast<int16_t>(src_offset_x),
                .src_offset_y = static_cast<int16_t>(src_offset_y),
                .dst_stride = static_cast<int16_t>(hal::kDisplayWidth),
                .dst_offset_x = static_cast<int16_t>(dst_offset_x),
                .dst_offset_y = static_cast<int16_t>(dst_offset_y),
                .width = static_cast<int16_t>(clipped_width),
                .height = static_cast<int16_t>(clipped_height),
                .rotation = hal::Rotation::k0,
            });
        }
    }
}

void
MapScreen::BlitToRotationBuffer()
{
    constexpr auto kNumTilesX = kMaxNumTilesX;
    constexpr auto kNumTilesY = kMaxNumTilesY;

    // Blit tiles directly into the oversized background buffer.
    uint16_t* bg = m_background.WritableData16();

    // Top-left of the oversized background buffer in OSM pixel coordinates
    const int start_x = m_current_view_center.x - kBgSize / 2;
    const int start_y = m_current_view_center.y - kBgSize / 2;

    m_blit_ops.clear();

    for (int y = 0; y < kNumTilesY; ++y)
    {
        const int tile_y = (start_y / kTileSize) + y;
        const int tile_pixel_y = tile_y * kTileSize;
        int32_t dst_offset_y = tile_pixel_y - start_y;
        int32_t src_offset_y = 0;

        int32_t clipped_height = kTileSize;
        clipped_height = std::min(clipped_height, kBgSize - dst_offset_y);

        if (dst_offset_y < 0)
        {
            src_offset_y = -dst_offset_y;
            clipped_height += dst_offset_y;
            dst_offset_y = 0;
        }
        if (clipped_height <= 0)
        {
            continue;
        }

        for (int x = 0; x < kNumTilesX; ++x)
        {
            const int tile_x = (start_x / kTileSize) + x;
            const int tile_pixel_x = tile_x * kTileSize;
            int32_t dst_offset_x = tile_pixel_x - start_x;
            int32_t src_offset_x = 0;
            int32_t clipped_width = kTileSize;

            if (dst_offset_x < 0)
            {
                src_offset_x = -dst_offset_x;
                clipped_width += dst_offset_x;
                dst_offset_x = 0;
            }
            clipped_width = std::min(clipped_width, kBgSize - dst_offset_x);

            if (clipped_width <= 0)
            {
                continue;
            }

            auto tile = m_tile_cache.GetTile(ToTile(Point {tile_pixel_x, tile_pixel_y, m_zoom}));

            m_blit_ops.push_back(hal::BlitOperation {
                .src_data = tile.Data16().data(),
                .dst_data = bg,
                .src_width = static_cast<int16_t>(tile.Width()),
                .src_height = static_cast<int16_t>(tile.Height()),
                .src_stride = static_cast<int16_t>(tile.Width()),
                .src_offset_x = static_cast<int16_t>(src_offset_x),
                .src_offset_y = static_cast<int16_t>(src_offset_y),
                .dst_stride = static_cast<int16_t>(kBgSize),
                .dst_height = static_cast<int16_t>(kBgSize),
                .dst_offset_x = static_cast<int16_t>(dst_offset_x),
                .dst_offset_y = static_cast<int16_t>(dst_offset_y),
                .width = static_cast<int16_t>(clipped_width),
                .height = static_cast<int16_t>(clipped_height),
                .rotation = hal::Rotation::k0,
            });
        }
    }
    m_parent.m_blitter.BlitOperations(
        std::span<const hal::BlitOperation> {m_blit_ops.data(), m_blit_ops.size()});
}

os::TimerHandle
MapScreen::StartHomeHoldTimer()
{
    return m_parent.StartTimer(2s, [this]() {
        auto position = OsmPointToWgs84(m_current_view_center);
        printf("Home position set to lat: %f, lon: %f\n", position.latitude, position.longitude);

        m_parent.m_state.CheckoutPartialSnapshot<AS::configuration>()
            .GetWritableReference<AS::configuration>()
            .home_position = position;

        return std::nullopt;
    });
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

    case hal::IInput::EventType::kTouchDown:
        m_touch_timer = m_parent.StartTimer(3s);
        m_home_hold_timer = StartHomeHoldTimer();

        m_last_touch_x = event.x;
        m_last_touch_y = event.y;
        break;
    case hal::IInput::EventType::kTouchMove: {
        m_touch_timer = m_parent.StartTimer(3s);
        m_home_hold_timer = StartHomeHoldTimer();

        m_current_view_center.x -= event.x - m_last_touch_x;
        m_current_view_center.y -= event.y - m_last_touch_y;
        m_last_touch_x = event.x;
        m_last_touch_y = event.y;
    }
    break;
    case hal::IInput::EventType::kTouchUp:
        m_home_hold_timer = nullptr;
        break;

    default:
        break;
    }
}
