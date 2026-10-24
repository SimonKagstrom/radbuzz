#pragma once

#include "application_state.hh"
#include "time.hh"

#include <lvgl.h>
#include <optional>
#include <string>

/**
 * The local time (HH:MM) in a tab at the top middle of the screen. Shown if enabled in the
 * configuration, and the time is known.
 */
class ClockWidget
{
public:
    explicit ClockWidget(lv_obj_t* parent);

    /**
     * Shown if show is true, the clock is enabled and the time is known.
     *
     * @return when to update again (since the UI doesn't listen to the clock), if shown
     */
    std::optional<milliseconds> Update(ApplicationState& state, bool show);

private:
    lv_obj_t* m_box {nullptr};
    lv_obj_t* m_label {nullptr};

    std::string m_text;
};
