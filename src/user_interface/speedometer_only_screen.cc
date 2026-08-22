#include "speedometer_only_screen.hh"

#include "map_screen.hh"
#include "painter.hh"
#include "radbuzz_font_120.h"
#include "radbuzz_font_22.h"
#include "radbuzz_font_40.h"
#include "radbuzz_numbers_font_16.h"
#include "time_string.hh"
#include "trip_utils.hh"

constexpr auto kMaxHistogramBarHeight = 150;
constexpr auto kHistogramBarWidth = 58;
constexpr auto kHistogramBarSpacing = kHistogramBarWidth + 7;

namespace
{

auto
CreateDatum(lv_obj_t* screen, const char* label_text, const char* value_text, const char* unit_text)
{
    SpeedometerOnlyScreen::Datum datum;

    datum.description_label = lv_label_create(screen);
    lv_obj_set_style_text_font(datum.description_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(datum.description_label, lv_color_white(), LV_PART_MAIN);

    datum.value_unit_label = lv_label_create(screen);
    lv_obj_set_style_text_font(datum.value_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(datum.value_unit_label, lv_color_white(), LV_PART_MAIN);

    datum.value_label = lv_label_create(screen);
    lv_obj_set_style_text_font(datum.value_label, &radbuzz_font_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(datum.value_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(datum.value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    lv_label_set_text(datum.description_label, label_text);
    lv_label_set_text(datum.value_label, value_text);
    lv_label_set_text(datum.value_unit_label, unit_text);

    return datum;
}

} // namespace

SpeedometerOnlyScreen::Datum
SpeedometerOnlyScreen::LeftAligned(const char* label_text,
                                   const char* value_text,
                                   const char* unit_text,
                                   lv_align_t alignment,
                                   Point offset)
{
    auto datum = CreateDatum(m_screen, label_text, value_text, unit_text);

    lv_obj_align(datum.description_label, alignment, offset.x, offset.y);
    lv_obj_align_to(datum.value_label, datum.description_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_align_to(datum.value_unit_label, datum.value_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -4);

    lv_obj_set_style_text_align(datum.value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    return datum;
}

SpeedometerOnlyScreen::Datum
SpeedometerOnlyScreen::CenterAligned(const char* label_text,
                                     const char* value_text,
                                     const char* unit_text,
                                     lv_align_t alignment,
                                     Point offset)
{
    auto datum = CreateDatum(m_screen, label_text, value_text, unit_text);

    lv_obj_set_style_text_align(datum.description_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_align(datum.value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_align(datum.value_unit_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    lv_obj_align(datum.description_label, alignment, offset.x, offset.y);
    lv_obj_align_to(datum.value_label, datum.description_label, LV_ALIGN_OUT_BOTTOM_MID, -16, 4);
    lv_obj_align_to(datum.value_unit_label, datum.value_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -4);

    return datum;
}

SpeedometerOnlyScreen::Datum
SpeedometerOnlyScreen::RightAligned(const char* label_text,
                                    const char* value_text,
                                    const char* unit_text,
                                    lv_align_t alignment,
                                    Point offset)
{
    auto datum = CreateDatum(m_screen, label_text, value_text, unit_text);

    lv_obj_set_style_text_align(datum.value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_align(datum.description_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_align(datum.value_unit_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    lv_obj_align(datum.description_label, alignment, offset.x, offset.y);
    lv_obj_align_to(
        datum.value_unit_label, datum.description_label, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 4 + 7);
    lv_obj_align_to(datum.value_label, datum.value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);

    return datum;
}

SpeedometerOnlyScreen::SpeedometerOnlyScreen(UserInterface& parent)
    : UserInterface::ScreenBase(parent, lv_obj_create(nullptr))
{
    const lv_color_t kBackgroundColor = lv_color_make(47, 47, 58);
    const lv_color_t kBarColor = lv_color_make(128, 128, 128);

    // Callback to draw histogram lines on the background
    lv_obj_add_event_cb(
        m_screen,
        [](lv_event_t* e) {
            auto* self = static_cast<SpeedometerOnlyScreen*>(lv_event_get_user_data(e));
            auto* layer = lv_event_get_layer(e);

            self->DrawHistogramLines(layer);
        },
        LV_EVENT_DRAW_MAIN,
        this);


    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_screen, kBackgroundColor, 0);

    // Recent power averages
    for (auto i = 0; i < TripComputer::kNumberOfRecentEntries; ++i)
    {
        auto bar = lv_obj_create(m_screen);
        lv_obj_set_size(bar, kHistogramBarWidth, 0);
        lv_obj_set_style_bg_color(bar, kBarColor, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, i * kHistogramBarSpacing, 0);

        m_recent_entry_bars.push_back(bar);
    }

    m_current_histogram_bar_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_current_histogram_bar_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_current_histogram_bar_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(m_current_histogram_bar_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(m_current_histogram_bar_label, "•");


    for (auto i = 0; i < m_recent_entry_labels.size(); ++i)
    {
        auto vertical = lv_label_create(m_screen);
        auto horizontal = lv_label_create(m_screen);

        lv_obj_set_style_text_font(vertical, &radbuzz_numbers_font_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(vertical, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(vertical, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

        lv_obj_set_style_text_font(horizontal, &radbuzz_numbers_font_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(horizontal, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(horizontal, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

        m_recent_entry_labels[i] = vertical;
        m_recent_entry_horizontal_labels[i] = horizontal;
    }
    lv_obj_align(
        m_recent_entry_labels[0], LV_ALIGN_BOTTOM_LEFT, 0, -kMaxHistogramBarHeight / 2 - 2);
    lv_obj_align(m_recent_entry_labels[1], LV_ALIGN_BOTTOM_LEFT, 0, -kMaxHistogramBarHeight - 2);

    lv_obj_align(m_recent_entry_horizontal_labels[0], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_align(m_recent_entry_horizontal_labels[1],
                 LV_ALIGN_BOTTOM_LEFT,
                 kHistogramBarSpacing * TripComputer::kNumberOfRecentEntries / 2,
                 0);

    lv_label_set_text(m_recent_entry_labels[0], "1800W");
    lv_label_set_text(m_recent_entry_labels[1], "1800W");

    // Big speedometer in the center of the screen
    m_speedometer_box = lv_obj_create(m_screen);

    constexpr int kCornerClipPx = 16;
    constexpr int kPaneCornerRadius = 18;

    lv_obj_set_size(m_speedometer_box, 304 + kCornerClipPx, 128 + 32);
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
    lv_obj_clear_flag(m_speedometer_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(m_speedometer_box, LV_ALIGN_TOP_MID, 0, 80);


    m_speedometer_label = lv_label_create(m_speedometer_box);
    lv_obj_align(m_speedometer_label, LV_ALIGN_CENTER, -16, 0);
    lv_obj_set_style_text_font(m_speedometer_label, &radbuzz_font_120, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speedometer_label, lv_color_white(), LV_PART_MAIN);

    m_speedometer_unit_label = lv_label_create(m_screen);
    lv_obj_align_to(m_speedometer_unit_label, m_speedometer_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
    lv_obj_set_style_text_font(m_speedometer_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speedometer_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_speedometer_unit_label, "km/h");

    m_battery = LeftAligned("Battery", "100", "%", LV_ALIGN_TOP_LEFT, {0, 4});
    m_temperature =
        RightAligned("Controller/Motor/BMS/Cell", "0/0/0/0", "°C", LV_ALIGN_TOP_RIGHT, {-16, 4});

    // Align both these to the longest realistic distance
    constexpr auto kMaxGoodLookingDistance = "99.9";
    m_trip_distance = RightAligned("Trip",
                                   kMaxGoodLookingDistance,
                                   "m",
                                   LV_ALIGN_BOTTOM_RIGHT,
                                   {-20, -kPixelSize_radbuzz_font_40 * 2});
    m_trip_time = RightAligned(
        "", kMaxGoodLookingDistance, "", LV_ALIGN_BOTTOM_RIGHT, {-14, -kPixelSize_radbuzz_font_40});

    m_range = LeftAligned("Range",
                          "999",
                          "km",
                          LV_ALIGN_TOP_LEFT,
                          {0, kPixelSize_radbuzz_font_40 + kPixelSize_radbuzz_font_22 + 10});

    m_power = CenterAligned("Power",
                            "1800",
                            "W",
                            LV_ALIGN_BOTTOM_MID,
                            {-110, -kMaxHistogramBarHeight - kPixelSize_radbuzz_font_40 - 16});
    m_consumption = CenterAligned("Consumption",
                                  "18.9",
                                  "Wh/km",
                                  LV_ALIGN_BOTTOM_MID,
                                  {80, -kMaxHistogramBarHeight - kPixelSize_radbuzz_font_40 - 16});

    // Never show
    lv_obj_set_flag(m_trip_time.description_label, LV_OBJ_FLAG_HIDDEN, true);
}

void
SpeedometerOnlyScreen::Update()
{
    lv_label_set_text(m_speedometer_label,
                      std::format("{}", m_parent.m_state.Get<AS::speed>()).c_str());

    lv_label_set_text(m_battery.value_label,
                      std::format("{}", m_parent.m_state.Get<AS::battery_soc>()).c_str());

    std::string temperature_text = "Controller";
    std::string temperature_value_text =
        std::format("{}", m_parent.m_state.Get<AS::controller_temperature>());

    if (m_parent.m_state.Get<AS::motor_temperature>() != 0)
    {
        temperature_text +=
            "/Motor" + std::to_string(m_parent.m_state.Get<AS::motor_temperature>());
        temperature_value_text +=
            "/" + std::to_string(m_parent.m_state.Get<AS::motor_temperature>());
    }
    if (auto bms = m_parent.m_state.Get<AS::bms_data>(); bms->valid)
    {
        temperature_text += "/BMS/Cell";
        temperature_value_text += "/" + std::to_string(bms->bms_temperature) + "/" +
                                  std::to_string(bms->highest_cell_temp);
    }

    lv_label_set_text(m_temperature.description_label, temperature_text.c_str());
    lv_label_set_text(m_temperature.value_label, temperature_value_text.c_str());

    auto power = m_parent.m_state.Get<AS::current_power_w>();
    std::string power_text;

    if (power < 1000)
    {
        power_text = std::format("{}", power);
        lv_label_set_text(m_power.value_unit_label, "W");
    }
    else
    {
        power_text = std::format("{:.1f}", power / 1000.0f);
        lv_label_set_text(m_power.value_unit_label, "kW");
    }
    lv_label_set_text(m_power.value_label, power_text.c_str());

    lv_label_set_text(
        m_consumption.value_label,
        std::format("{:.1f}",
                    trip::AverageConsumption(m_parent.m_state, m_parent.m_current_trip_start))
            .c_str());

    lv_label_set_text(m_range.value_label,
                      std::format("{}", m_parent.m_state.Get<AS::estimated_range_km>()).c_str());

    auto trip_duration = m_parent.m_state.Get<AS::trip_duration>();
    std::string trip_time_text = SecondsToString(trip_duration);

    lv_label_set_text(m_trip_time.value_label, trip_time_text.c_str());

    std::string trip_value_text;
    auto trip_distance_m = m_parent.m_state.Get<AS::trip_distance>();

    if (trip_distance_m < 1000)
    {
        trip_value_text = std::format("{}", trip_distance_m);
        lv_label_set_text(m_trip_distance.value_unit_label, "m");
    }
    else
    {
        trip_value_text = std::format("{:.1f}", trip_distance_m / 1000.0f);
        lv_label_set_text(m_trip_distance.value_unit_label, "km");
    }
    lv_label_set_text(m_trip_distance.value_label, trip_value_text.c_str());


    auto recent_entries = m_parent.m_trip_computer.GetRecentEntries();
    debug_assert(recent_entries.size() == m_recent_entry_bars.size());

    auto conf = m_parent.m_state.Get<AS::configuration>();
    const float max_watts = conf->max_watts;
    const float max_consumption = conf->wh_per_km_for_range_estimation + 10;

    if (conf->histogram_mode == HistogramMode::kPower)
    {
        lv_label_set_text(m_recent_entry_labels[0],
                          std::format("{:4} W", static_cast<int>(max_watts / 2)).c_str());
        lv_label_set_text(m_recent_entry_labels[1],
                          std::format("{:4} W", static_cast<int>(max_watts)).c_str());

        for (size_t i = 0; i < m_recent_entry_bars.size(); ++i)
        {
            lv_obj_set_size(
                m_recent_entry_bars[i],
                kHistogramBarSpacing,
                static_cast<int>(recent_entries[i].power / max_watts * kMaxHistogramBarHeight));
        }

        lv_obj_align(
            m_current_histogram_bar_label,
            LV_ALIGN_BOTTOM_LEFT,
            kHistogramBarSpacing * (TripComputer::kNumberOfRecentEntries - 1) +
                kHistogramBarWidth / 2,
            -static_cast<int>(recent_entries.back().power / max_watts * kMaxHistogramBarHeight -
                              kPixelSize_radbuzz_font_22 / 2));
    }
    else
    {
        lv_label_set_text(m_recent_entry_labels[0],
                          std::format("{} Wh", static_cast<int>(max_consumption / 2)).c_str());
        lv_label_set_text(m_recent_entry_labels[1],
                          std::format("{} Wh", static_cast<int>(max_consumption)).c_str());

        for (size_t i = 0; i < m_recent_entry_bars.size(); ++i)
        {
            lv_obj_set_size(m_recent_entry_bars[i],
                            kHistogramBarSpacing,
                            static_cast<int>(recent_entries[i].average_consumption /
                                             max_consumption * kMaxHistogramBarHeight));
        }
    }

    lv_label_set_text(
        m_recent_entry_horizontal_labels[0],
        std::format("-{} m", conf->recent_power_distance * TripComputer::kNumberOfRecentEntries)
            .c_str());

    lv_label_set_text(
        m_recent_entry_horizontal_labels[1],
        std::format("-{} m", conf->recent_power_distance * TripComputer::kNumberOfRecentEntries / 2)
            .c_str());


    // Dynamic alignment
    lv_obj_align_to(
        m_battery.value_label, m_battery.value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);
    lv_obj_align_to(m_power.value_label, m_power.value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);
    lv_obj_align_to(m_range.value_label, m_range.value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);
    lv_obj_align_to(m_trip_distance.value_unit_label,
                    m_trip_distance.description_label,
                    LV_ALIGN_OUT_BOTTOM_RIGHT,
                    0,
                    4 + 7);
    lv_obj_align_to(m_trip_distance.value_label,
                    m_trip_distance.value_unit_label,
                    LV_ALIGN_OUT_LEFT_BOTTOM,
                    -4,
                    4);
    lv_obj_align_to(
        m_trip_time.value_label, m_trip_time.value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);
    lv_obj_align_to(
        m_temperature.value_label, m_temperature.value_unit_label, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);
}

void
SpeedometerOnlyScreen::DrawHistogramLines(lv_layer_t* layer)
{
    auto* dst = static_cast<uint16_t*>(static_cast<void*>(layer->draw_buf->data));

    constexpr auto kWidth = kHistogramBarWidth * (TripComputer::kNumberOfRecentEntries + 1) + 8;

    const auto kLineColor = lv_color_to_u16(lv_color_white());
    const auto kLineEndColor = lv_color_to_u16(lv_color_make(128, 128, 128));


    painter::DrawClippedHorizontalLine<Point, 1, painter::LineStyle::kDashed>(
        dst, {64, hal::kDisplayHeight - kMaxHistogramBarHeight}, kWidth - 16, kLineColor);
    painter::DrawClippedHorizontalLine<Point, 1, painter::LineStyle::kDashed>(
        dst, {kWidth - 16, hal::kDisplayHeight - kMaxHistogramBarHeight}, kWidth, kLineEndColor);

    painter::DrawClippedHorizontalLine<Point, 1, painter::LineStyle::kDashed>(
        dst, {64, hal::kDisplayHeight - kMaxHistogramBarHeight / 2}, kWidth - 16, kLineColor);
    painter::DrawClippedHorizontalLine<Point, 1, painter::LineStyle::kDashed>(
        dst,
        {kWidth - 16, hal::kDisplayHeight - kMaxHistogramBarHeight / 2},
        kWidth,
        kLineEndColor);
}

void
SpeedometerOnlyScreen::HandleInput(const Input::Event& event)
{
    int dx = 0;
    auto map_screen = static_cast<MapScreen*>(m_parent.m_map_screen.get());

    switch (event.type)
    {
    case hal::IInput::EventType::kLeft:
        dx = -1;
        break;
    case hal::IInput::EventType::kRight:
        dx = 1;
        break;
    case hal::IInput::EventType::kButtonDown:
        m_parent.ActivateScreen(*m_parent.m_settings_menu_screen);
        return;
    default:
        break;
    }


    debug_assert(m_parent.m_lvgl_touch_input_dev);

    if (m_parent.m_touch_state == LV_INDEV_STATE_PRESSED)
    {
        lv_point_t touch_vector {0, 0};

        lv_indev_get_vect(m_parent.m_lvgl_touch_input_dev, &touch_vector);

        // Swipes anywhere
        const auto gesture_dir = lv_indev_get_gesture_dir(m_parent.m_lvgl_touch_input_dev);
        dx = 1 * (gesture_dir == LV_DIR_LEFT) - 1 * (gesture_dir == LV_DIR_RIGHT);
    }

    if (dx == -1)
    {
        map_screen->SetZoom(kDefaultZoom);
        m_parent.ActivateScreen(*m_parent.m_map_screen);
    }
    else if (dx == 1)
    {
        m_parent.ActivateScreen(*m_parent.m_trip_meter_screen);
    }
}

void
SpeedometerOnlyScreen::SetHelp(bool on)
{
    if (!on)
    {
        m_explanatory_bubbles.clear();
        return;
    }

    m_explanatory_bubbles.push_back(
        std::make_unique<SpeechBubble>(m_temperature.description_label,
                                       SpeechBubble::Direction::kLeft,
                                       "Temperatures of the controller etc,\nif available",
                                       Point {0, 16}));
    m_explanatory_bubbles.push_back(std::make_unique<SpeechBubble>(m_trip_time.value_label,
                                                                   SpeechBubble::Direction::kLeft,
                                                                   "Current trip time and distance",
                                                                   Point {0, 0}));
}
