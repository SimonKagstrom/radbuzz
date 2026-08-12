#include "user_interface.hh"

#include "indicators.hh"
#include "map_screen.hh"
#include "ota_update_screen.hh"
#include "painter.hh"
#include "settings_menu_screen.hh"
#include "trip_meter_screen.hh"

#include <radbuzz_font_22.h>
#include <radbuzz_symbols_40.h>

enum IndicatorType
{
    kBattery,
    kOverheated,
    kHome,
    kGpsLost,
    kPaused,
    kWifi,
    kBluetooth,

    kValueCount,
};


UserInterface::IndicatorBase::IndicatorBase(UserInterface& parent, const Point& position)
    : m_parent(parent)
    , m_indicator_label(lv_label_create(lv_layer_top()))
{

    lv_obj_set_style_text_font(m_indicator_label, &radbuzz_symbols_40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_indicator_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_recolor(m_indicator_label, true);
    lv_label_set_text(m_indicator_label, "");

    lv_obj_set_pos(m_indicator_label, position.x, position.y);
}

UserInterface::UserInterface(hal::IDisplay& display,
                             hal::IBlitter& blitter,
                             std::unique_ptr<hal::IPm::ILock> pm_lock,
                             hal::IInput& input,
                             OtaUpdater& ota_updater,
                             ApplicationState& state,
                             ImageCache& cache,
                             TileCache& tile_cache,
                             TripComputer& trip_computer)
    : m_display(display)
    , m_blitter(blitter)
    , m_pm_lock(std::move(pm_lock))
    , m_input(input)
    , m_ota_updater(ota_updater)
    , m_state(state)
    , m_image_cache(cache)
    , m_tile_cache(tile_cache)
    , m_trip_computer(trip_computer)
    , m_state_cache(m_state)
{
    // Probably should listen to a few others, but many are bulk-updated.
    m_state_listener = m_state.AttachListener<AS::pixel_position,
                                              AS::battery_soc,
                                              AS::odometer,
                                              AS::bluetooth_connected,
                                              AS::wifi_connected,
                                              AS::speed,
                                              AS::tile_loaded,
                                              AS::trip_duration,
                                              AS::navigation_active,
                                              AS::is_moving,
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
    auto f_rotate = m_display.GetFrameBuffer(hal::IDisplay::Owner::kRotationBuffer);

    // Preconfigure the blitter operations for rotation, apart from src/dst
    m_rotation_blit_operations[0] = {
        .src_data = nullptr, // Set on flush
        .dst_data = f_rotate,
        .src_width = static_cast<int16_t>(hal::kDisplayWidth),
        .src_height = static_cast<int16_t>(hal::kDisplayHeight),
        .src_offset_x = 0,
        .src_offset_y = 0,
        .dst_offset_x = 0,
        .dst_offset_y = 0,
        .width = static_cast<int16_t>(hal::kDisplayWidth),
        .height = static_cast<int16_t>(hal::kDisplayHeight),
        .rotation = hal::kDisplayRotation,
    };

    // Blit back to the sw-owned buffer (i.e., no rotation)
    m_rotation_blit_operations[1] = {
        .src_data = f_rotate,
        .dst_data = nullptr, // Set on flush
        .src_width = static_cast<int16_t>(hal::kDisplayWidth),
        .src_height = static_cast<int16_t>(hal::kDisplayHeight),
        .src_offset_x = 0,
        .src_offset_y = 0,
        .dst_offset_x = 0,
        .dst_offset_y = 0,
        .width = static_cast<int16_t>(hal::kDisplayWidth),
        .height = static_cast<int16_t>(hal::kDisplayHeight),
        .rotation = hal::Rotation::k0,
    };

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
                auto frame_buffer = reinterpret_cast<uint16_t*>(px_map);

                p->DrawPowerBar(frame_buffer);
                if constexpr (hal::kDisplayRotation != hal::Rotation::k0)
                {
                    p->m_rotation_blit_operations[0].src_data = frame_buffer;
                    p->m_rotation_blit_operations[1].dst_data = frame_buffer;

                    p->m_blitter.BlitOperations(
                        std::span<const hal::BlitOperation> {p->m_rotation_blit_operations.data(),
                                                             p->m_rotation_blit_operations.size()});
                    p->m_blitter.WaitForBlitsDone();
                }

                p->m_display.Flip();
            }
            lv_display_flush_ready(display);
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

    m_lvgl_touch_input_dev = lv_indev_create();
    lv_indev_set_mode(m_lvgl_touch_input_dev, LV_INDEV_MODE_EVENT);
    lv_indev_set_type(m_lvgl_touch_input_dev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(m_lvgl_touch_input_dev, this);
    lv_indev_set_read_cb(m_lvgl_touch_input_dev, [](lv_indev_t* indev, lv_indev_data_t* data) {
        auto p = static_cast<UserInterface*>(lv_indev_get_user_data(indev));

        data->state = p->m_touch_state;
        data->point = p->m_touch_point;
    });

    m_map_screen = std::make_unique<MapScreen>(*this, m_image_cache, m_tile_cache, kDefaultZoom);
    m_trip_meter_screen = std::make_unique<TripMeterScreen>(*this);
    m_settings_menu_screen = std::make_unique<SettingsMenuScreen>(*this);
    m_ota_update_screen = std::make_unique<OtaUpdateScreen>(*this);

    m_screens = {m_map_screen.get(),
                 m_trip_meter_screen.get(),
                 m_settings_menu_screen.get(),
                 m_ota_update_screen.get()};

    // Keep this widget above any active screen (map, trip meter, settings, ...).
    m_digital_speedometer = std::make_unique<DigitalSpeedometerWidget>(lv_layer_top());

    // The battery icon is at the top right, and the rest in a column to the lright
    constexpr auto kIndicatorRowSpacing = 46;
    constexpr auto kIndicatorColumn = hal::kDisplayWidth - kPowerBarWidth - 48;
    auto indicator_row_y = DigitalSpeedometerWidget::kBoxDimensions - 46;


    m_indicators.resize(std::to_underlying(IndicatorType::kValueCount));
    m_indicators[IndicatorType::kBattery] = std::make_unique<BatteryIndicator>(
        *this, Point {hal::kDisplayWidth - DigitalSpeedometerWidget::kBoxDimensions - 72, 0});

    m_indicators[IndicatorType::kOverheated] = std::make_unique<OverheatedIndicator>(
        *this, Point {kIndicatorColumn, indicator_row_y += kIndicatorRowSpacing});
    m_indicators[IndicatorType::kHome] = std::make_unique<HomeIndicator>(
        *this, Point {kIndicatorColumn, indicator_row_y += kIndicatorRowSpacing});
    m_indicators[IndicatorType::kGpsLost] = std::make_unique<GpsLostIndicator>(
        *this, Point {kIndicatorColumn, indicator_row_y += kIndicatorRowSpacing});
    m_indicators[IndicatorType::kPaused] = std::make_unique<PausedIndicator>(
        *this, Point {kIndicatorColumn, indicator_row_y += kIndicatorRowSpacing});
    m_indicators[IndicatorType::kWifi] = std::make_unique<WifiIndicator>(
        *this, Point {kIndicatorColumn, indicator_row_y += kIndicatorRowSpacing});
    m_indicators[IndicatorType::kBluetooth] = std::make_unique<BluetoothIndicator>(
        *this, Point {kIndicatorColumn, indicator_row_y += kIndicatorRowSpacing});

    ActivateScreen(*m_map_screen);
    ResetTrip();

    // Allow placing the objects first, so delay a bit
    m_show_help_timer = StartTimer(10ms, [this]() {
        if (m_state.Get<AS::configuration>()->show_speech_bubbles)
        {
            ShowHelp();
        }

        return std::nullopt;
    });

    m_show_all_indicators_timer = StartTimer(3s, [this]() {
        HideHelp();
        return std::nullopt;
    });
}

void
UserInterface::SetHelp(bool on)
{
    for (auto& screen : m_screens)
    {
        screen->SetHelp(on);
    }

    if (!on)
    {
        m_explanatory_bubbles.clear();
        return;
    }


    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_indicators[IndicatorType::kOverheated]->m_indicator_label,
                                       SpeechBubble::Direction::kLeft,
                                       "Controller/battery overheat warning"));

    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_indicators[IndicatorType::kHome]->m_indicator_label,
                                       SpeechBubble::Direction::kLeft,
                                       "Home range warning"));

    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_indicators[IndicatorType::kGpsLost]->m_indicator_label,
                                       SpeechBubble::Direction::kLeft,
                                       "GPS lost warning"));
    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_indicators[IndicatorType::kPaused]->m_indicator_label,
                                       SpeechBubble::Direction::kLeft,
                                       "Stationary, trip paused"));

    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_indicators[IndicatorType::kBluetooth]->m_indicator_label,
                                       SpeechBubble::Direction::kLeft,
                                       "Bluetooth navigation\nconnected"));

    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_digital_speedometer->m_boxes[1],
                                       SpeechBubble::Direction::kLeft,
                                       "Trip distance and\ntime"));


    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_digital_speedometer->m_boxes[0],
                                       SpeechBubble::Direction::kRight,
                                       "Speedometer (km/h),\nGPS speed (small)"));
}


// Tesla style gray + black / green power bar
void
UserInterface::DrawPowerBar(uint16_t* dst)
{
    const auto kBackgroundColor = lv_color_to_u16(lv_color_make(100, 100, 100));
    const auto kShadowColor = lv_color_to_u16(lv_color_make(135, 135, 135));
    const auto kPositivePowerColor = lv_color_to_u16(lv_color_black());
    const auto kNegativePowerColor = lv_color_to_u16(lv_palette_main(LV_PALETTE_GREEN));

    auto ro = m_state.CheckoutReadonly();
    auto conf = ro.Get<AS::configuration>();

    auto height = hal::kDisplayHeight;
    auto y_start = 0;
    if (OnMapScreen())
    {
        if (ro.Get<AS::navigation_active>())
        {
            height -= MapScreen::kNavigationBoxHeight;
        }
        // The distance box is only shown on the map
        y_start = DigitalSpeedometerWidget::kBoxDimensions;
    }

    const int pixels_at_max_power = height / 2;

    const auto watts_signed = static_cast<int16_t>(ro.Get<AS::current_power_w>());
    const int abs_watts = std::abs(watts_signed);
    const int max_watts = std::max(1, static_cast<int>(conf->max_watts));
    const int power_bar_size = (abs_watts * pixels_at_max_power) / max_watts;
    const int clamped_power_bar_size = std::min(power_bar_size, pixels_at_max_power);

    Point from {hal::kDisplayWidth - kPowerBarWidth,
                hal::kDisplayHeight / 2 - clamped_power_bar_size};


    auto bar_color = kPositivePowerColor;
    if (watts_signed < 0)
    {
        bar_color = kNegativePowerColor;
    }

    painter::DrawClippedVerticalLine<Point, kPowerBarWidth>(
        dst, {hal::kDisplayWidth - kPowerBarWidth, y_start}, height, kBackgroundColor);
    // Shadow line to make it more clear
    painter::DrawClippedVerticalLine<Point, 1>(
        dst, {hal::kDisplayWidth - kPowerBarWidth - 1, y_start}, height, kShadowColor);
    painter::DrawClippedVerticalLine<Point, kPowerBarWidth>(dst, from, height / 2, bar_color);
}


void
UserInterface::ResetTrip()
{
    m_state.CheckoutReadWrite().Post<AS::reset_trip>();

    auto ro = m_state.CheckoutReadonly();

    // These are only in the user interface
    m_current_trip_start = {
        ro.Get<AS::wh_consumed>(),
        ro.Get<AS::wh_regenerated>(),
    };
}

std::optional<milliseconds>
UserInterface::OnActivation()
{
    auto& co = m_state_cache.Pull();
    if (co.IsChanged<AS::pixel_position>())
    {
        m_distance_home_meters =
            MetersBetweenPoints(m_state.CheckoutReadonly().Get<AS::configuration>()->home_position,
                                co.Get<AS::pixel_position>());
    }

    Input::Event input_event;

    while (m_input_queue.pop(input_event))
    {
        auto event = input_event.type;

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

        case hal::IInput::EventType::kTouchDown:
        case hal::IInput::EventType::kTouchMove:
            m_touch_state = LV_INDEV_STATE_PRESSED;
            m_touch_point = {
                static_cast<int32_t>(input_event.x),
                static_cast<int32_t>(input_event.y),
            };
            lv_indev_read(m_lvgl_touch_input_dev);
            break;
        case hal::IInput::EventType::kTouchUp:
            m_touch_state = LV_INDEV_STATE_RELEASED;
            m_touch_point = {
                static_cast<int32_t>(input_event.x),
                static_cast<int32_t>(input_event.y),
            };
            lv_indev_read(m_lvgl_touch_input_dev);
            break;
        default:
            break;
        }

        m_current_screen->HandleInput(input_event);
    }

    auto max_power = m_pm_lock->FullPower();

    m_current_screen->Update();
    m_current_screen->UpdateHelp();

    m_digital_speedometer->Update(m_state, OnMapScreen());
    for (auto& indicator : m_indicators)
    {
        indicator->Update(m_state);
    }

    // Like in automobiles, show all indicators at startup
    if (!m_show_all_indicators_timer->IsExpired())
    {
        for (auto& indicator : m_indicators)
        {
            lv_obj_clear_flag(indicator->m_indicator_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Show help, if it's active
    for (auto& bubble : m_explanatory_bubbles)
    {
        bubble->Update();
    }

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
