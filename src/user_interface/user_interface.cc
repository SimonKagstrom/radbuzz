#include "user_interface.hh"

#include "map_screen.hh"
#include "painter.hh"
#include "trip_meter_screen.hh"

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
    m_state_listener = m_state.AttachListener<AS::position,
                                              AS::battery_soc,
                                              AS::distance_traveled,
                                              AS::bluetooth_connected,
                                              AS::wifi_connected,
                                              AS::wh_consumed,
                                              AS::wh_regenerated>(GetSemaphore());
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

    m_map_screen = std::make_unique<MapScreen>(*this, m_image_cache, m_tile_cache);
    m_trip_meter_screen = std::make_unique<TripMeterScreen>(*this);

    m_map_screen->Activate();
    m_current_screen = m_map_screen.get();

    //    m_menu_screen = std::make_unique<MenuScreen>(
    //        GetTimerManager(), m_lvgl_input_dev, []() { printf("Menu closed\n"); });

    static auto vobb = StartTimer(5s, [this]() {
        auto now = m_current_screen;

        if (now == m_map_screen.get())
        {
            m_trip_meter_screen->Activate();
            m_current_screen = m_trip_meter_screen.get();
        }
        else
        {
            m_map_screen->Activate();
            m_current_screen = m_map_screen.get();
        }

        return 5s;
    });
}

std::optional<milliseconds>
UserInterface::OnActivation()
{
    auto max_power = m_pm_lock->FullPower();

    m_current_screen->Update();

    if (auto time_before = os::GetTimeStampRaw(); m_next_redraw_time > time_before)
    {
        // Wait for the next redraw
        return milliseconds(m_next_redraw_time - time_before);
    }
    auto delay = lv_timer_handler();
    m_next_redraw_time = os::GetTimeStampRaw() + delay;

    if (lv_display_get_screen_loading(m_lvgl_display))
    {
        return milliseconds(delay);
    }

    return std::nullopt;
}
