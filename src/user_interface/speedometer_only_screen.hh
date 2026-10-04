#pragma once

#include "trip_computer.hh"
#include "user_interface.hh"

#include <etl/vector.h>

class SpeedometerOnlyScreen : public UserInterface::ScreenBase
{
public:
    // How the description and value/unit are aligned to each other
    enum class DatumAlignment
    {
        kLeft,
        kCenter,
        kRight,
    };

    /*
     * A description, with a value + unit under it. Kept in a container (sized after the
     * contents), which is placed as a whole.
     */
    struct Datum
    {
        lv_obj_t* container {nullptr};
        lv_obj_t* description_label {nullptr};
        lv_obj_t* value_label {nullptr};
        lv_obj_t* value_unit_label {nullptr};

        void SetHidden(bool hidden)
        {
            lv_obj_set_flag(container, LV_OBJ_FLAG_HIDDEN, hidden);
        }

        // Place relative to the screen. Kept by LVGL when the size changes
        void Align(lv_align_t align, Point offset)
        {
            m_base = nullptr;
            lv_obj_align(container, align, offset.x, offset.y);
        }

        // Place relative to another object (e.g., LV_ALIGN_OUT_BOTTOM_MID)
        void AlignTo(lv_obj_t* base, lv_align_t align, Point offset)
        {
            m_base = base;
            m_align = align;
            m_offset = offset;
            Refresh();
        }

        // LVGL only aligns to other objects once, so redo it when the size might have changed
        void Refresh()
        {
            if (m_base)
            {
                lv_obj_align_to(container, m_base, m_align, m_offset.x, m_offset.y);
            }
        }

    private:
        lv_obj_t* m_base {nullptr};
        lv_align_t m_align {LV_ALIGN_DEFAULT};
        Point m_offset {0, 0};
    };

    explicit SpeedometerOnlyScreen(UserInterface& parent);

private:
    void Update() final;
    void HandleInput(const Input::Event& event) final;
    void SetHelp(bool on) final;

    bool ShowsNavigation() const final
    {
        return true;
    }

    // Place it with Align() or AlignTo() afterwards
    Datum CreateDatum(DatumAlignment alignment,
                      const char* label_text,
                      const char* value_text,
                      const char* unit_text);

    void DrawHistogramLines(lv_layer_t* layer);

    // Move/hide things to make room for the side pane and the navigation widget
    void Layout(bool side_pane_shown, bool navigating);

    lv_obj_t* m_speedometer_box {nullptr};

    lv_obj_t* m_speedometer_label {nullptr};
    lv_obj_t* m_speedometer_unit_label {nullptr};

    lv_obj_t* m_small_speedometer_label {nullptr};
    lv_obj_t* m_small_speedometer_unit_label {nullptr};

    // Parent of the histogram bars and labels
    lv_obj_t* m_histogram {nullptr};
    etl::vector<lv_obj_t*, TripComputer::kNumberOfRecentEntries> m_recent_entry_bars {};
    std::array<lv_obj_t*, 2> m_recent_entry_labels {};
    std::array<lv_obj_t*, 2> m_recent_entry_horizontal_labels {};

    lv_obj_t* m_current_histogram_bar_label {nullptr};

    Datum m_battery;
    Datum m_temperature;
    Datum m_trip_distance;
    Datum m_trip_time;
    Datum m_range;
    Datum m_power;
    Datum m_consumption;

    lv_obj_t* m_consumption_label {nullptr};

    bool m_laid_out_for_side_pane {false};
    bool m_laid_out_for_navigation {false};
};
