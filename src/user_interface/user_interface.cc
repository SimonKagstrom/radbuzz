#include "user_interface.hh"

#include "map_screen.hh"
#include "painter.hh"
#include "settings_menu_screen.hh"
#include "trip_meter_screen.hh"

#include <radbuzz_font_22.h>

UserInterface::UserInterface(hal::IDisplay& display,
                             std::unique_ptr<hal::IPm::ILock> pm_lock,
                             hal::IInput& input,
                             ApplicationState& state,
                             ImageCache& cache,
                             TileCache& tile_cache)
    : m_display(display)
    , m_pm_lock(std::move(pm_lock))
    , m_input(input)
    , m_state(state)
    , m_image_cache(cache)
    , m_tile_cache(tile_cache)
    , m_state_cache(m_state)
{
    m_state_listener = m_state.AttachListener<AS::position,
                                              AS::battery_soc,
                                              AS::distance_traveled,
                                              AS::bluetooth_connected,
                                              AS::wifi_connected,
                                              AS::speed,
                                              AS::wh_consumed,
                                              AS::wh_regenerated>(GetSemaphore());
    m_cache_listener = m_image_cache.ListenToChanges(GetSemaphore());

    // Context: Interrupt/anoteher thread
    m_input_listener = m_input.AttachListener([this](auto event) {
        m_input_queue.push(event);
        Awake();
    });
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
    lv_indev_set_read_cb(m_lvgl_input_dev, [](lv_indev_t* indev, lv_indev_data_t* data) {
        auto p = static_cast<UserInterface*>(lv_indev_get_user_data(indev));

        data->state = p->m_button_state;
        data->enc_diff = p->m_enc_diff;
    });

    m_map_screen = std::make_unique<MapScreen>(*this, m_image_cache, m_tile_cache);
    m_trip_meter_screen = std::make_unique<TripMeterScreen>(*this);
    m_settings_menu_screen = std::make_unique<SettingsMenuScreen>(*this);

    ActivateScreen(*m_map_screen);
}

std::optional<milliseconds>
UserInterface::OnActivation()
{
    hal::IInput::EventType event;

    while (m_input_queue.pop(event))
    {
        printf("VOBB : Processing input event %d\n", static_cast<int>(event));

        m_enc_diff = 0;

        switch (event)
        {
        case hal::IInput::EventType::kButtonDown:
            m_button_state = LV_INDEV_STATE_PRESSED;
            break;
        case hal::IInput::EventType::kButtonUp:
            m_button_state = LV_INDEV_STATE_RELEASED;
            break;
        case hal::IInput::EventType::kLeft:
            m_enc_diff = -1;
            break;
        case hal::IInput::EventType::kRight:
            m_enc_diff = 1;
            break;
        default:
            break;
        }

        m_current_screen->HandleInput(event);
    }

    // Set the pixel position (since the UI can move it around in the future)
    const auto& co = m_state_cache.Pull();

    co.OnNewValue<AS::position>([&](const auto& gps_data) {
        if (auto pixel_pos = Wgs84ToOsmPoint(gps_data.position, kDefaultZoom); pixel_pos)
        {
            m_state.CheckoutReadWrite().Set<AS::pixel_position>(*pixel_pos);
            auto overview_point = OsmPointToPoint(*pixel_pos, kDefaultZoom - 2);
            m_tile_cache.GetTile(ToTile(overview_point));
        }
    });

    auto max_power = m_pm_lock->FullPower();

    m_current_screen->Update();

    if (auto time_before = os::GetTimeStampRaw(); m_next_redraw_time > time_before)
    {
        // Wait for the next redraw
        return milliseconds(m_next_redraw_time - time_before);
    }
    auto delay = lv_timer_handler();
    m_next_redraw_time = os::GetTimeStampRaw() + delay;

    if (lv_display_get_screen_loading(m_lvgl_display) ||
        // Half a second of activity on input (for animations)
        lv_display_get_inactive_time(m_lvgl_display) < 500)
    {
        return milliseconds(delay);
    }

    return std::nullopt;
}
