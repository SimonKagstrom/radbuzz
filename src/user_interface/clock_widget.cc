#include "clock_widget.hh"

#include "clock_string.hh"
#include "ui_style.hh"

namespace
{

// Pushed above the screen, so that only the lower corners are rounded (like a tab)
constexpr int32_t kCornerRadius = 12;
constexpr int32_t kHorizontalPadding = 12;
constexpr int32_t kBottomPadding = 4;

// The time arrives from the phone or GPS, which the UI doesn't listen to
constexpr auto kInvalidRecheckTime = 5s;

} // namespace

ClockWidget::ClockWidget(lv_obj_t* parent)
    : m_box(ui::CreatePane(parent, kCornerRadius))
    , m_label(ui::CreateLabel(m_box))
{
    lv_obj_set_size(m_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(m_box, kCornerRadius, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(m_box, kBottomPadding, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(m_box, kHorizontalPadding, LV_PART_MAIN);
    lv_obj_align(m_box, LV_ALIGN_TOP_MID, 0, -kCornerRadius);

    lv_obj_set_hidden(m_box, true);
}

std::optional<milliseconds>
ClockWidget::Update(ApplicationState& state, bool show)
{
    if (!show || !state.Get<AS::configuration>()->show_clock)
    {
        lv_obj_set_hidden(m_box, true);
        return std::nullopt;
    }

    // Not known yet
    const auto utc = state.Get<AS::clock>();
    if (utc == 0)
    {
        lv_obj_set_hidden(m_box, true);
        return kInvalidRecheckTime;
    }

    // Only redraw when the minute changes
    if (auto text = ClockToString(utc, state.Get<AS::timezone>()); text != m_text)
    {
        m_text = std::move(text);
        lv_label_set_text(m_label, m_text.c_str());
    }
    lv_obj_set_hidden(m_box, false);

    return seconds(SecondsToNextMinute(utc));
}
