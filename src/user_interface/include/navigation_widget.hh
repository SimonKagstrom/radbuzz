#pragma once

#include "application_state.hh"
#include "hal/i_display.hh"

#include <lvgl.h>

/**
 * The navigation box (turn icon + distance) at the bottom left, and the next street to the right
 * of it. Only shown when navigation is active.
 */
class NavigationWidget
{
public:
    static constexpr auto kDescriptionBoxHeight = 32;

    explicit NavigationWidget(lv_obj_t* parent);

    /**
     * Shown if show is true and navigation is active.
     *
     * @param left_x where the widget starts, e.g., to the right of the side pane
     */
    void Update(ApplicationState& state, bool show, int32_t left_x);

    // For help bubbles
    lv_obj_t* GetTurnSymbolLabel() const
    {
        return m_current_turn_symbol;
    }

    lv_obj_t* GetDescriptionLabel() const
    {
        return m_description_label;
    }

private:
    void SetLeftX(int32_t left_x);

    lv_obj_t* m_left_box {nullptr};
    lv_obj_t* m_navigation_box {nullptr};
    lv_obj_t* m_navigation_description_box {nullptr};
    lv_obj_t* m_current_turn_symbol {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    int32_t m_left_x {0};
};
