#include "user_interface.hh"

#include "painter.hh"

#include <radbuzz_font_22.h>

UserInterface::UserInterface(hal::IDisplay& display,
                             std::unique_ptr<hal::IPm::ILock> pm_lock,
                             ApplicationState& state,
                             ImageCache& cache,
                             TileCache& tile_cache)
    : m_display(display)
    , m_pm_lock(std::move(pm_lock))
    , m_state(state)
    , m_image_cache(cache)
    , m_tile_cache(tile_cache)
{
    m_static_map_buffer = std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(
        aligned_alloc(64, hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t))));
    m_static_map_image = std::make_unique<Image>(
        std::span<const uint8_t> {m_static_map_buffer.get(),
                                  hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t)},
        hal::kDisplayWidth,
        hal::kDisplayHeight);


    m_state_listener = m_state.AttachListener<AS::position>(GetSemaphore());
    m_cache_listener = m_image_cache.ListenToChanges(GetSemaphore());
}

void
UserInterface::OnStartup()
{
    assert(m_lvgl_display == nullptr);

    lv_init();
    lv_tick_set_cb(os::GetTimeStampRaw);

    m_lvgl_display = lv_display_create(hal::kDisplayWidth, hal::kDisplayHeight);
    auto f1 = m_display.GetFrameBuffer(hal::IDisplay::Owner::kSoftware);
    auto f2 = m_display.GetFrameBuffer(hal::IDisplay::Owner::kHardware);

    lv_display_set_buffers(m_lvgl_display,
                           f1,
                           f2,
                           sizeof(uint16_t) * hal::kDisplayWidth * hal::kDisplayHeight,
                           lv_display_render_mode_t::LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(m_lvgl_display, this);
    lv_display_set_flush_cb(
        m_lvgl_display,
        [](lv_display_t* display, const lv_area_t* area [[maybe_unused]], uint8_t* px_map) {
            if (lv_display_flush_is_last(display))
            {
                auto p = static_cast<UserInterface*>(lv_display_get_user_data(display));

                p->m_display.Flip();
                lv_display_flush_ready(display);
            }
        });

    m_lvgl_input_dev = lv_indev_create();
    lv_indev_set_mode(m_lvgl_input_dev, LV_INDEV_MODE_EVENT);
    lv_indev_set_type(m_lvgl_input_dev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_user_data(m_lvgl_input_dev, this);
    //lv_indev_set_read_cb(m_lvgl_input_dev, StaticLvglEncoderRead);

    m_screen = lv_image_create(nullptr);

    lv_obj_move_background(m_screen);
    lv_image_set_src(m_screen, &m_static_map_image->lv_image_dsc);
    lv_screen_load(m_screen);

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


    //    m_menu_screen = std::make_unique<MenuScreen>(
    //        GetTimerManager(), m_lvgl_input_dev, []() { printf("Menu closed\n"); });
}

std::optional<milliseconds>
UserInterface::OnActivation()
{
    auto max_power = m_pm_lock->FullPower();

    auto state = m_state.CheckoutReadonly();
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

    lv_label_set_text(m_description_label, std::format("{}", *state.Get<AS::next_street>()).c_str());
    lv_label_set_text(m_distance_left_label, std::format("{} m", state.Get<AS::distance_to_next>()).c_str());

    // TMP!
    lv_label_set_text(m_description_label,
                      std::format("Controller: {}°C, Battery: {:.1f}V",
                                  state.Get<AS::controller_temperature>(),
                                  static_cast<float>(state.Get<AS::battery_millivolts>()) / 1000.0f)
                          .c_str());

    if (auto time_before = os::GetTimeStampRaw(); m_next_redraw_time > time_before)
    {
        // Wait for the next redraw
        return milliseconds(m_next_redraw_time - time_before);
    }
    auto delay = lv_timer_handler();
    m_next_redraw_time = os::GetTimeStampRaw() + delay;

    return milliseconds(delay);
}
