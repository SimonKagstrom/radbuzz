#include "ui_style.hh"

#include "user_interface.hh"

namespace
{

struct Styles
{
    Styles()
    {
        lv_style_init(&pane);
        lv_style_set_bg_color(&pane, lv_color_black());
        lv_style_set_bg_opa(&pane, LV_OPA_COVER);
        lv_style_set_border_width(&pane, 0);

        lv_style_init(&label);
        lv_style_set_text_color(&label, lv_color_white());

        lv_style_init(&screen);
        lv_style_set_bg_color(&screen, UserInterface::GetBackgroundColor());
    }

    lv_style_t pane;
    lv_style_t label;
    lv_style_t screen;
};

const Styles&
GetStyles()
{
    // On first use, i.e., after lv_init()
    static const Styles styles;

    return styles;
}

} // namespace

lv_obj_t*
ui::CreatePane(lv_obj_t* parent, int32_t radius)
{
    auto obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &GetStyles().pane, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_clickable(obj, false);

    return obj;
}

lv_obj_t*
ui::CreateContainer(lv_obj_t* parent)
{
    auto obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);

    return obj;
}

lv_obj_t*
ui::CreateLabel(lv_obj_t* parent, const lv_font_t* font, const char* text)
{
    auto label = lv_label_create(parent);

    lv_obj_add_style(label, &GetStyles().label, LV_PART_MAIN);
    if (font)
    {
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    }
    lv_label_set_text(label, text);

    return label;
}

void
ui::StyleScreen(lv_obj_t* screen)
{
    lv_obj_add_style(screen, &GetStyles().screen, LV_PART_MAIN);
    lv_obj_set_scrollable(screen, false);
}
