#include "message_box.hh"

#include "lv_event_listener.hh"

MessageBox::MessageBox(lv_indev_t* encoder,
                       const std::string& title,
                       const std::string& text,
                       std::vector<Button> buttons)
    : m_encoder(encoder)
    , m_previous_group(lv_indev_get_group(encoder))
    , m_group(lv_group_create())
    , m_box(lv_msgbox_create(nullptr))
    , m_buttons(std::move(buttons))
{
    lv_msgbox_add_title(m_box, title.c_str());
    lv_msgbox_add_text(m_box, text.c_str());

    for (const auto& button : m_buttons)
    {
        auto btn = lv_msgbox_add_footer_button(m_box, button.text.c_str());

        lv_group_add_obj(m_group, btn);
        LvEventListener::Create(btn, LV_EVENT_CLICKED, [this, &button](lv_event_t*) {
            button.on_click();
            Close();
        });
    }

    // Let the encoder select among the buttons
    lv_indev_set_group(m_encoder, m_group);
}

MessageBox::~MessageBox()
{
    if (m_open)
    {
        lv_indev_set_group(m_encoder, m_previous_group);
        lv_msgbox_close(m_box);
    }
    lv_group_delete(m_group);
}

bool
MessageBox::IsOpen() const
{
    return m_open;
}

void
MessageBox::Close()
{
    if (!m_open)
    {
        return;
    }

    m_open = false;
    lv_indev_set_group(m_encoder, m_previous_group);

    // Async, since we're called from the event handler of one of the buttons
    lv_msgbox_close_async(m_box);
}
