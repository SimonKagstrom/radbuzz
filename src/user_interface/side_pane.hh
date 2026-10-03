#pragma once

#include "user_interface.hh"

#include <cstdint>
#include <lvgl.h>
#include <string>
#include <vector>

/**
 * A pane at the left side of the screen, showing a queue of messages. The encoder scrolls
 * through them as one long text.
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

    // Queued last, or updated if a message with the same id is already queued
    void AddMessage(const Message& message);

    // E.g., dismissed on the phone
    void RemoveMessage(uint32_t id);

    // Dismiss the shown message, and show the next one (if any)
    void Dismiss();

    // Shown if there is a message, and it's not suppressed (e.g., during a call)
    bool IsShown() const;
    void SetSuppressed(bool suppressed);

    // Is the point (in screen coordinates) within the pane?
    bool Contains(int32_t x, int32_t y) const;

    // Encoder input (touch is handled by LVGL)
    void HandleInput(const Input::Event& event);

    bool HasMessages() const
    {
        return !m_messages.empty();
    }

private:
    // Show m_messages[m_current], at the start or (when going backwards) the end of the text
    void ShowCurrentMessage(bool at_end = false);
    // Does nothing at the first/last message (no wrapping)
    void ShowNextMessage();
    void ShowPreviousMessage(bool at_end);
    // Touch swipes between messages
    void OnGesture();

    void RemoveMessageAt(size_t index);

    // Scroll by dy pixels (positive = further down in the text)
    void Scroll(int32_t dy);
    // The scroll position, including a running scroll animation, has reached the start/end
    bool AtTop() const;
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

    bool m_suppressed {false};

    std::vector<Message> m_messages;
    // The shown message
    size_t m_current {0};
};
