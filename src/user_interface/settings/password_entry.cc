#include "password_entry.hh"

#include "lv_event_listener.hh"

PasswordEntry::PasswordEntry(lv_indev_t* encoder, std::string_view ssid)
    : m_encoder(encoder)
    , m_previous_group(lv_indev_get_group(encoder))
    , m_group(lv_group_create())
    , m_backdrop(lv_obj_create(lv_layer_top()))
    , m_last_input_tick(lv_tick_get())
{
    // Dim the menu, which stays visible above the panel
    lv_obj_remove_style_all(m_backdrop);
    lv_obj_set_size(m_backdrop, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(m_backdrop, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m_backdrop, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_clickable(m_backdrop, true);

    // The title, password and keyboard in the lower part of the screen
    auto panel = lv_obj_create(m_backdrop);
    lv_obj_set_size(panel, lv_pct(100), lv_pct(62));
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(panel, false);

    auto title = lv_label_create(panel);
    lv_obj_set_width(title, lv_pct(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title, (std::string("Password for ") + std::string(ssid)).c_str());

    auto password_row = lv_obj_create(panel);
    lv_obj_remove_style_all(password_row);
    lv_obj_set_size(password_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(password_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        password_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(password_row, 8, LV_PART_MAIN);

    m_textarea = lv_textarea_create(password_row);
    lv_obj_set_flex_grow(m_textarea, 1);
    lv_textarea_set_one_line(m_textarea, true);
    lv_textarea_set_password_mode(m_textarea, true);
    lv_textarea_set_max_length(m_textarea, kMaxPasswordLength);
    // Show the cursor, although the keyboard has the (encoder) focus
    lv_obj_add_state(m_textarea, LV_STATE_FOCUSED);

    auto show_button = lv_button_create(password_row);
    m_show_label = lv_label_create(show_button);
    lv_label_set_text(m_show_label, LV_SYMBOL_EYE_OPEN);
    LvEventListener::Create(
        show_button, LV_EVENT_CLICKED, [this](lv_event_t*) { ToggleShowPassword(); });

    auto keyboard = lv_keyboard_create(panel);
    lv_obj_set_width(keyboard, lv_pct(100));
    lv_obj_set_flex_grow(keyboard, 1);
    lv_keyboard_set_textarea(keyboard, m_textarea);

    // OK and enter (one line), and the close/keyboard keys, are sent to the text area
    LvEventListener::Create(m_textarea, LV_EVENT_READY, [this](lv_event_t*) { Accept(); });
    LvEventListener::Create(m_textarea, LV_EVENT_CANCEL, [this](lv_event_t*) { Cancel(); });
    LvEventListener::Create(keyboard, LV_EVENT_VALUE_CHANGED, [this](lv_event_t*) {
        m_last_input_tick = lv_tick_get();
    });

    // The encoder selects keys (editing, since the keyboard is the only object)
    lv_group_add_obj(m_group, keyboard);
    lv_group_set_editing(m_group, true);
    lv_indev_set_group(m_encoder, m_group);
}

PasswordEntry::~PasswordEntry()
{
    lv_indev_set_group(m_encoder, m_previous_group);
    lv_obj_delete(m_backdrop);
    lv_group_delete(m_group);
}

PasswordEntry::Result
PasswordEntry::GetResult() const
{
    return m_result;
}

const std::string&
PasswordEntry::Password() const
{
    return m_password;
}

bool
PasswordEntry::IsIdle() const
{
    return lv_tick_elaps(m_last_input_tick) > kIdleTimeoutMs;
}

void
PasswordEntry::Accept()
{
    std::string password = lv_textarea_get_text(m_textarea);

    // Open networks don't need a password, so this is probably a mistake
    if (password.empty() || m_result != Result::kPending)
    {
        return;
    }

    m_password = std::move(password);
    m_result = Result::kAccepted;
}

void
PasswordEntry::Cancel()
{
    if (m_result == Result::kPending)
    {
        m_result = Result::kCancelled;
    }
}

void
PasswordEntry::ToggleShowPassword()
{
    m_last_input_tick = lv_tick_get();

    const auto hidden = !lv_textarea_get_password_mode(m_textarea);
    lv_textarea_set_password_mode(m_textarea, hidden);
    lv_label_set_text(m_show_label, hidden ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}
