#pragma once

#include <functional>
#include <lvgl.h>
#include <string>
#include <vector>

/**
 * A modal message box, which can be controlled both by touch and the rotary encoder
 * (rotate to select a button, press to click it).
 */
class MessageBox final
{
public:
    struct Button
    {
        std::string text;
        std::function<void()> on_click {[]() {}};
    };

    // The box is closed when any of the buttons are clicked
    MessageBox(lv_indev_t* encoder,
               const std::string& title,
               const std::string& text,
               std::vector<Button> buttons);

    ~MessageBox();

    // The button callbacks refer to this
    MessageBox(const MessageBox&) = delete;
    MessageBox& operator=(const MessageBox&) = delete;

    bool IsOpen() const;

private:
    void Close();

    lv_indev_t* m_encoder;
    lv_group_t* m_previous_group;
    lv_group_t* m_group;
    lv_obj_t* m_box;

    std::vector<Button> m_buttons;
    bool m_open {true};
};
