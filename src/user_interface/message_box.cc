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
    lv_obj_set_style_radius(m_box, 8, LV_PART_MAIN);

    lv_msgbox_add_title(m_box, title.c_str());
    lv_msgbox_add_text(m_box, text.c_str());

    lv_obj_set_style_pad_hor(lv_msgbox_get_header(m_box), 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lv_msgbox_get_content(m_box), 16, LV_PART_MAIN);

    static lv_style_t style_fixed_btn;

    lv_style_init(&style_fixed_btn);
    lv_style_set_width(&style_fixed_btn, 90);
    lv_style_set_height(&style_fixed_btn, 45);

    for (const auto& button : m_buttons)
    {
        auto btn = lv_msgbox_add_footer_button(m_box, button.text.c_str());
        lv_obj_add_style(btn, &style_fixed_btn, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);

        lv_group_add_obj(m_group, btn);
        LvEventListener::Create(btn, LV_EVENT_CLICKED, [this, &button](lv_event_t*) {
            button.on_click();
            Close();
        });
    }

    if (auto footer = lv_msgbox_get_footer(m_box))
    {
        // The default footer height is smaller than the buttons
        lv_obj_set_height(footer, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(footer, 16, LV_PART_MAIN);
        lv_obj_set_style_pad_column(footer, 16, LV_PART_MAIN);
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
