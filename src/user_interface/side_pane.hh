#pragma once

#include "user_interface.hh"

#include <cstdint>
#include <lvgl.h>
#include <string>

/**
 * A pane at the left side of the screen, showing a message.
 */
class SidePane
{
public:
    struct Message
    {
        uint32_t id {0};
        std::string source; // The app, e.g., "gmail"
        std::string title;
        std::string body;
    };

    // The part of the screen (left of the power bar). Change to e.g., / 2 to experiment
    static constexpr int32_t kWidth = (hal::kDisplayWidth - kPowerBarWidth) / 3;

    explicit SidePane(lv_obj_t* parent);
    ~SidePane();

    SidePane(const SidePane&) = delete;
    SidePane& operator=(const SidePane&) = delete;

    // For now, replaces the shown message
    void AddMessage(const Message& message);

    void RemoveMessage(uint32_t id);

    // Dismiss the shown message
    void Dismiss();

    // Shown if there is a message, and it's not suppressed (e.g., during a call)
    bool IsShown() const;
    void SetSuppressed(bool suppressed);

    // Is the point (in screen coordinates) within the pane?
    bool Contains(int32_t x, int32_t y) const;

    // Encoder input (touch is handled by LVGL)
    void HandleInput(const Input::Event& event);

private:
    // Scroll by dy pixels (positive = further down in the text)
    void Scroll(int32_t dy);
    // The scroll position, including a running scroll animation, has reached the end
    bool AtBottom() const;
    // Gray until the end of the message has been reached (for the encoder)
    void UpdateOkButton();
    void UpdateVisibility();

    lv_obj_t* m_pane {nullptr};
    lv_obj_t* m_title_label {nullptr};
    lv_obj_t* m_body {nullptr};
    lv_obj_t* m_body_label {nullptr};
    lv_obj_t* m_ok_button {nullptr};
    lv_obj_t* m_counter_label {nullptr};

    bool m_has_message {false};
    uint32_t m_message_id {0};
    bool m_suppressed {false};
};
