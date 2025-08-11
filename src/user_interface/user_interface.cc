#include "user_interface.hh"

UserInterface::UserInterface(hal::IDisplay& display, ApplicationState& state, ImageCache& cache)
    : m_display(display)
    , m_state(state)
    , m_image_cache(cache)
{
    m_state_listener = m_state.AttachListener(GetSemaphore());
    m_cache_listener = m_image_cache.ListenToChanges(GetSemaphore());
}

void
UserInterface::OnStartup()
{
    assert(m_lvgl_display == nullptr);

    lv_init();
    lv_tick_set_cb(os::GetTimeStampRaw);

    m_lvgl_display = lv_display_create(hal::kDisplayWidth, hal::kDisplayHeight);
    auto f1 = m_display.GetFrameBuffer(hal::IDisplay::Owner::kSoftware);
    auto f2 = m_display.GetFrameBuffer(hal::IDisplay::Owner::kHardware);

    lv_display_set_buffers(m_lvgl_display,
                           f1,
                           f2,
                           sizeof(uint16_t) * hal::kDisplayWidth * hal::kDisplayHeight,
                           lv_display_render_mode_t::LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(m_lvgl_display, this);
    lv_display_set_flush_cb(
        m_lvgl_display,
        [](lv_display_t* display, const lv_area_t* area [[maybe_unused]], uint8_t* px_map) {
            if (lv_display_flush_is_last(display))
            {
                auto p = static_cast<UserInterface*>(lv_display_get_user_data(display));

                p->m_display.Flip();
                lv_display_flush_ready(display);
            }
        });


    m_screen = lv_obj_create(nullptr);
    lv_screen_load(m_screen);


    m_current_icon = lv_image_create(m_screen);
    lv_obj_center(m_current_icon);
    lv_image_set_src(m_current_icon, &m_image_cache.Lookup(kInvalidIconHash)->GetDsc());
    lv_obj_align(m_current_icon, LV_ALIGN_CENTER, 0, 0);

    m_description_label = lv_label_create(m_screen);
    lv_obj_align(m_description_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(m_description_label, &lv_font_montserrat_22, LV_PART_MAIN);
}

std::optional<milliseconds>
UserInterface::OnActivation()
{
    auto state = m_state.CheckoutReadonly();
    auto state_hash = state->current_icon_hash;

    if (m_current_icon_hash != state_hash)
    {
        if (auto image = m_image_cache.Lookup(state_hash); image)
        {
            m_current_icon_hash = state_hash;
            lv_image_set_src(m_current_icon, &image->GetDsc());
        }
    }

    lv_label_set_text(m_description_label, std::format("{}", state->next_street).c_str());

    if (auto time_before = os::GetTimeStampRaw(); m_next_redraw_time > time_before)
    {
        // Wait for the next redraw
        return milliseconds(m_next_redraw_time - time_before);
    }
    auto delay = lv_timer_handler();
    m_next_redraw_time = os::GetTimeStampRaw() + delay;

    return milliseconds(delay);
}
