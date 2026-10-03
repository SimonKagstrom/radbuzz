#include "speedometer_only_screen.hh"

#include "map_screen.hh"
#include "navigation_widget.hh"
#include "painter.hh"
#include "radbuzz_font_120.h"
#include "radbuzz_font_22.h"
#include "radbuzz_font_40.h"
#include "radbuzz_numbers_font_16.h"
#include "side_pane.hh"
#include "time_string.hh"
#include "trip_utils.hh"

constexpr auto kMaxHistogramBarHeight = 150;
constexpr auto kHistogramBarWidth = 58;
constexpr auto kHistogramBarSpacing = kHistogramBarWidth + 7;

constexpr auto kSpeedometerBoxY = 80;
// With the side pane: centered between the pane and the indicator icons (and vertically)
constexpr auto kSpeedometerBoxSidePaneXOffset =
    (SidePane::kWidth + kIndicatorColumn) / 2 - hal::kDisplayWidth / 2;

constexpr Point kBatteryPosition {0, 4};
// Just right of the side pane
constexpr Point kBatterySidePanePosition {SidePane::kWidth + 8, 4};

// The bottom of the power/consumption descriptions, above the histogram
constexpr auto kPowerDescriptionBottom =
    hal::kDisplayHeight - kMaxHistogramBarHeight - kPixelSize_radbuzz_font_40 - 16;
constexpr auto kPowerX = -110;

// The trip distance/time at the bottom right. When navigating, on a single line above the
// navigation description box, with the distance growing leftwards from the time
constexpr Point kTripDistancePosition {-20, -kPixelSize_radbuzz_font_40};
constexpr Point kTripTimePosition {-10, 0};
// Only moved up, above the navigation description box
constexpr Point kTripTimeNavigationPosition {kTripTimePosition.x,
                                             -NavigationWidget::kDescriptionBoxHeight - 4};
constexpr Point kTripDistanceNavigationOffset {-20, 0};
constexpr auto kConsumptionX = 80;

namespace
{

// A transparent container, sized after the contents
lv_obj_t*
CreateContainer(lv_obj_t* parent, lv_flex_flow_t flow)
{
    auto obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(obj, flow);

    return obj;
}

lv_obj_t*
CreateLabel(lv_obj_t* parent, const lv_font_t* font, const char* text)
{
    auto label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(label, text);

    return label;
}

// The top of the power/consumption datums
int32_t
PowerY()
{
    return kPowerDescriptionBottom - lv_font_get_line_height(&radbuzz_font_22);
}

} // namespace

SpeedometerOnlyScreen::Datum
SpeedometerOnlyScreen::CreateDatum(DatumAlignment alignment,
                                   const char* label_text,
                                   const char* value_text,
                                   const char* unit_text)
{
    Datum datum;

    const auto cross_alignment = alignment == DatumAlignment::kLeft     ? LV_FLEX_ALIGN_START
                                 : alignment == DatumAlignment::kCenter ? LV_FLEX_ALIGN_CENTER
                                                                        : LV_FLEX_ALIGN_END;

    // The description, with the value + unit under it
    datum.container = CreateContainer(m_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(datum.container, LV_FLEX_ALIGN_START, cross_alignment, cross_alignment);
    lv_obj_set_style_pad_row(datum.container, 4, LV_PART_MAIN);

    datum.description_label = CreateLabel(datum.container, &radbuzz_font_22, label_text);

    // The bottom of the unit and value aligned
    auto value_row = CreateContainer(datum.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(value_row, 4, LV_PART_MAIN);

    // At least as wide as the initial text, and right aligned in that. Keeps the unit in place
    // for shorter values
    datum.value_label = CreateLabel(value_row, &radbuzz_font_40, value_text);
    lv_obj_set_style_text_align(datum.value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_update_layout(datum.value_label);
    lv_obj_set_style_min_width(
        datum.value_label, lv_obj_get_width(datum.value_label), LV_PART_MAIN);
    datum.value_unit_label = CreateLabel(value_row, &radbuzz_font_22, unit_text);
    // Slightly above the bottom of the (larger) value font
    lv_obj_set_style_pad_bottom(datum.value_unit_label, 4, LV_PART_MAIN);

    return datum;
}

SpeedometerOnlyScreen::SpeedometerOnlyScreen(UserInterface& parent)
    : UserInterface::ScreenBase(parent, lv_obj_create(nullptr))
{
    const lv_color_t kBackgroundColor = lv_color_make(47, 47, 58);
    const lv_color_t kBarColor = lv_color_make(128, 128, 128);

    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_screen, kBackgroundColor, 0);

    // The histogram (bars, labels and lines), so that it can be hidden as a whole. Covers the
    // screen, so that the alignments are the same as for the screen
    m_histogram = lv_obj_create(m_screen);
    lv_obj_set_size(m_histogram, hal::kDisplayWidth, hal::kDisplayHeight);
    lv_obj_set_pos(m_histogram, 0, 0);
    lv_obj_set_style_bg_opa(m_histogram, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_histogram, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_histogram, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(m_histogram, 0, LV_PART_MAIN);
    lv_obj_clear_flag(m_histogram, LV_OBJ_FLAG_SCROLLABLE);
    // Let touch (swipes) through to the screen
    lv_obj_clear_flag(m_histogram, LV_OBJ_FLAG_CLICKABLE);

    // Callback to draw histogram lines on the background (not called when hidden)
    lv_obj_add_event_cb(
        m_histogram,
        [](lv_event_t* e) {
            auto* self = static_cast<SpeedometerOnlyScreen*>(lv_event_get_user_data(e));
            auto* layer = lv_event_get_layer(e);

            self->DrawHistogramLines(layer);
        },
        LV_EVENT_DRAW_MAIN,
        this);

    // Recent power averages
    for (auto i = 0; i < TripComputer::kNumberOfRecentEntries; ++i)
    {
        auto bar = lv_obj_create(m_histogram);
        lv_obj_set_size(bar, kHistogramBarWidth, 0);
        lv_obj_set_style_bg_color(bar, kBarColor, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, i * kHistogramBarSpacing, 0);

        m_recent_entry_bars.push_back(bar);
    }

    m_current_histogram_bar_label = lv_label_create(m_histogram);
    lv_obj_set_style_text_font(m_current_histogram_bar_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_current_histogram_bar_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(m_current_histogram_bar_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(m_current_histogram_bar_label, "•");


    for (auto i = 0; i < m_recent_entry_labels.size(); ++i)
    {
        auto vertical = lv_label_create(m_histogram);
        auto horizontal = lv_label_create(m_histogram);

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


    m_speedometer_label = lv_label_create(m_speedometer_box);
    lv_obj_align(m_speedometer_label, LV_ALIGN_CENTER, -16, 0);
    lv_obj_set_style_text_font(m_speedometer_label, &radbuzz_font_120, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speedometer_label, lv_color_white(), LV_PART_MAIN);

    m_small_speedometer_label = lv_label_create(m_speedometer_box);
    lv_obj_align_to(
        m_small_speedometer_label, m_speedometer_label, LV_ALIGN_OUT_BOTTOM_MID, 16, 10);
    lv_obj_set_style_text_font(m_small_speedometer_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_small_speedometer_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_small_speedometer_label, "19");

    m_speedometer_unit_label = lv_label_create(m_speedometer_box);
    lv_obj_align_to(m_speedometer_unit_label, m_speedometer_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
    lv_obj_set_style_text_font(m_speedometer_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_speedometer_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_speedometer_unit_label, "km/h");
    lv_obj_set_style_text_align(m_speedometer_unit_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    m_small_speedometer_unit_label = lv_label_create(m_speedometer_box);
    lv_obj_align_to(
        m_small_speedometer_unit_label, m_speedometer_unit_label, LV_ALIGN_OUT_BOTTOM_RIGHT, 4, 10);
    lv_obj_set_style_text_font(m_small_speedometer_unit_label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_small_speedometer_unit_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_small_speedometer_unit_label, "GPS");
    lv_obj_set_style_text_align(m_small_speedometer_unit_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);


    m_battery = CreateDatum(DatumAlignment::kLeft, "Battery", "100", "%");
    m_range = CreateDatum(DatumAlignment::kLeft, "Range", "999", "km");
    m_range.Align(LV_ALIGN_TOP_LEFT,
                  {0, kPixelSize_radbuzz_font_40 + kPixelSize_radbuzz_font_22 + 10});

    m_temperature =
        CreateDatum(DatumAlignment::kRight, "Controller/Motor/BMS/Cell", "0/0/0/0", "°C");
    m_temperature.Align(LV_ALIGN_TOP_RIGHT, {-16, 4});

    // Align both these to the longest realistic distance
    constexpr auto kMaxGoodLookingDistance = "99.9";
    m_trip_distance = CreateDatum(DatumAlignment::kRight, "Trip", kMaxGoodLookingDistance, "m");
    // "Trip" is only shown here when navigating, the trip distance has it otherwise
    m_trip_time = CreateDatum(DatumAlignment::kRight, "Trip", kMaxGoodLookingDistance, "");
    // At the same x as when it's above the trip distance, so that it only moves vertically
    lv_obj_set_style_translate_x(m_trip_time.description_label,
                                 kTripDistancePosition.x - kTripTimePosition.x,
                                 LV_PART_MAIN);

    m_power = CreateDatum(DatumAlignment::kCenter, "Power", "1800", "W");
    m_consumption = CreateDatum(DatumAlignment::kCenter, "Consumption", "18.9", "Wh/km");
    m_consumption.Align(LV_ALIGN_TOP_MID, {kConsumptionX, PowerY()});

    // The battery, power, speedometer and trip are moved with the side pane and navigation
    Layout(false, false);

}

void
SpeedometerOnlyScreen::Update()
{
    auto conf = m_parent.m_state.Get<AS::configuration>();

    if (m_parent.m_state.Get<AS::gps_status>() == GpsStatus::kPositionValid)
    {
        lv_label_set_text(m_small_speedometer_label,
                          std::format("{}", m_parent.m_state.Get<AS::position>()->speed).c_str());
    }
    else
    {
        lv_label_set_text(m_small_speedometer_label, "--");
    }

    lv_label_set_text(m_speedometer_label,
                      std::format("{}", m_parent.m_state.Get<AS::speed>()).c_str());

    lv_label_set_text(m_battery.value_label,
                      std::format("{}", m_parent.m_state.Get<AS::battery_soc>()).c_str());

    lv_obj_set_flag(m_small_speedometer_label, LV_OBJ_FLAG_HIDDEN, !conf->show_gps_speed);
    lv_obj_set_flag(m_small_speedometer_unit_label, LV_OBJ_FLAG_HIDDEN, !conf->show_gps_speed);

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


    const auto side_pane_shown = m_parent.SidePaneShown();
    const auto navigating = m_parent.m_state.Get<AS::navigation_active>();
    if (side_pane_shown != m_laid_out_for_side_pane || navigating != m_laid_out_for_navigation)
    {
        Layout(side_pane_shown, navigating);
    }

    auto recent_entries = m_parent.m_trip_computer.GetRecentEntries();
    debug_assert(recent_entries.size() == m_recent_entry_bars.size());

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


    // Re-align the datums placed relative to other objects, since the values change size
    for (auto datum : {&m_battery,
                       &m_temperature,
                       &m_trip_distance,
                       &m_trip_time,
                       &m_range,
                       &m_power,
                       &m_consumption})
    {
        datum->Refresh();
    }
}

void
SpeedometerOnlyScreen::Layout(bool side_pane_shown, bool navigating)
{
    m_laid_out_for_side_pane = side_pane_shown;
    m_laid_out_for_navigation = navigating;

    // The navigation widget covers the bottom left part of the histogram
    lv_obj_set_flag(m_histogram, LV_OBJ_FLAG_HIDDEN, side_pane_shown || navigating);
    m_range.SetHidden(side_pane_shown);
    m_consumption.SetHidden(side_pane_shown);

    // A single line above the navigation description box, with "Trip" above the time
    m_trip_time.Align(LV_ALIGN_BOTTOM_RIGHT,
                      navigating ? kTripTimeNavigationPosition : kTripTimePosition);
    lv_obj_set_flag(m_trip_distance.description_label, LV_OBJ_FLAG_HIDDEN, navigating);
    lv_obj_set_flag(m_trip_time.description_label, LV_OBJ_FLAG_HIDDEN, !navigating);
    if (navigating)
    {
        m_trip_distance.AlignTo(
            m_trip_time.container, LV_ALIGN_OUT_LEFT_BOTTOM, kTripDistanceNavigationOffset);
    }
    else
    {
        m_trip_distance.Align(LV_ALIGN_BOTTOM_RIGHT, kTripDistancePosition);
    }

    if (side_pane_shown)
    {
        lv_obj_align(m_speedometer_box, LV_ALIGN_CENTER, kSpeedometerBoxSidePaneXOffset, 0);
        m_battery.Align(LV_ALIGN_TOP_LEFT, kBatterySidePanePosition);
        m_power.AlignTo(m_speedometer_box, LV_ALIGN_OUT_BOTTOM_MID, {0, 8});
    }
    else
    {
        lv_obj_align(m_speedometer_box, LV_ALIGN_TOP_MID, 0, kSpeedometerBoxY);
        m_battery.Align(LV_ALIGN_TOP_LEFT, kBatteryPosition);
        m_power.Align(LV_ALIGN_TOP_MID, {kPowerX, PowerY()});
    }
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
                                       "Temperatures of the\ncontroller etc,\nif available",
                                       Point {0, 20}));
    m_explanatory_bubbles.push_back(std::make_unique<SpeechBubble>(m_trip_time.value_label,
                                                                   SpeechBubble::Direction::kLeft,
                                                                   "Current trip time and distance",
                                                                   Point {0, 0}));
}
