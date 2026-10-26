#pragma once

#include <lvgl.h>
#include <string>
#include <string_view>

/**
 * A modal password entry with the on-screen keyboard, covering the lower part of the
 * (rectangular) screen. Controlled by touch, or the rotary encoder.
 *
 * The owner checks GetResult(), and destroys it when done, idle or no longer wanted (e.g.,
 * when moving). It can't close itself, since that happens from its own event callbacks.
 */
class PasswordEntry final
{
public:
    enum class Result
    {
        kPending,
        kAccepted,
        kCancelled,
    };

    PasswordEntry(lv_indev_t* encoder, std::string_view ssid);
    ~PasswordEntry();

    // The event callbacks refer to this
    PasswordEntry(const PasswordEntry&) = delete;
    PasswordEntry& operator=(const PasswordEntry&) = delete;

    Result GetResult() const;

    // When accepted
    const std::string& Password() const;

    // No input for a while, so probably forgotten
    bool IsIdle() const;

private:
    static constexpr uint32_t kIdleTimeoutMs = 60 * 1000;
    // WPA2 passphrases, or a hex key
    static constexpr uint32_t kMaxPasswordLength = 64;

    void Accept();
    void Cancel();
    void ToggleShowPassword();

    lv_indev_t* m_encoder;
    lv_group_t* m_previous_group;
    lv_group_t* m_group;

    // Covers the whole screen, so that touch doesn't reach the menu below
    lv_obj_t* m_backdrop;
    lv_obj_t* m_textarea {nullptr};
    lv_obj_t* m_show_label {nullptr};

    Result m_result {Result::kPending};
    std::string m_password;
    uint32_t m_last_input_tick;
};
