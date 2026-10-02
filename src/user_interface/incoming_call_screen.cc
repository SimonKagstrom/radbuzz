#include "incoming_call_screen.hh"

#include "lv_event_listener.hh"

#include <radbuzz_font_22.h>
#include <radbuzz_turn_symbols_60.h>

namespace
{

constexpr auto kMargin = 16;
constexpr auto kButtonHeight = 96;

// Keep clear of the indicator column on the right (and the power bar)
constexpr auto kUsableWidth = hal::kDisplayWidth - kPowerBarWidth - 48 - kMargin;
constexpr auto kButtonWidth = (kUsableWidth - 3 * kMargin) / 2;

// Center in the area left of the power bar
constexpr auto kCenterXOffset = -kPowerBarWidth / 2;

constexpr auto kIconY = 16;
constexpr auto kNameY = kIconY + 60 + 24;
constexpr auto kNumberY = kNameY + 22 + 12;

// "call" (0xe0b0) from the turn symbols font
constexpr auto kPhoneIcon = "\xEE\x82\xB0";

} // namespace

IncomingCallScreen::IncomingCallScreen(UserInterface& parent)
    : UserInterface::ScreenBase(parent, lv_obj_create(nullptr))
{
    const lv_color_t kBackgroundColor = lv_color_make(47, 47, 58);

    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_screen, kBackgroundColor, LV_PART_MAIN);
    lv_obj_clear_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);

    m_phone_icon_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_phone_icon_label, &radbuzz_turn_symbols_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(m_phone_icon_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(m_phone_icon_label, kPhoneIcon);
    lv_obj_align(m_phone_icon_label, LV_ALIGN_TOP_MID, kCenterXOffset, kIconY);

    for (auto label : {&m_name_label, &m_number_label})
    {
        *label = lv_label_create(m_screen);
        lv_obj_set_style_text_font(*label, &radbuzz_font_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(*label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(*label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        // Fixed width, so that long names are cut off and stay centered
        lv_obj_set_width(*label, hal::kDisplayWidth - kPowerBarWidth - 2 * kMargin);
        lv_label_set_long_mode(*label, LV_LABEL_LONG_DOT);
        lv_label_set_text(*label, "");
    }
    lv_obj_align(m_name_label, LV_ALIGN_TOP_MID, kCenterXOffset, kNameY);
    lv_obj_align(m_number_label, LV_ALIGN_TOP_MID, kCenterXOffset, kNumberY);

    // Like on phones: decline to the left, accept to the right
    CreateButton(Choice::kDecline, "Decline", lv_palette_main(LV_PALETTE_RED));
    CreateButton(Choice::kAccept, "Accept", lv_palette_main(LV_PALETTE_GREEN));
}

lv_obj_t*
IncomingCallScreen::CreateButton(Choice choice, const char* text, lv_color_t color)
{
    const auto index = std::to_underlying(choice);

    auto button = lv_button_create(m_screen);
    lv_obj_set_size(button, kButtonWidth, kButtonHeight);
    lv_obj_set_style_radius(button, 16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_align(
        button, LV_ALIGN_BOTTOM_LEFT, kMargin + index * (kButtonWidth + kMargin), -kMargin);

    // Selection with the encoder
    lv_obj_set_style_outline_color(button, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(button, 6, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(button, 4, LV_STATE_FOCUSED);

    auto label = lv_label_create(button);
    lv_obj_set_style_text_font(label, &radbuzz_font_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    // Touch
    LvEventListener::Create(
        button, LV_EVENT_CLICKED, [this, choice](lv_event_t*) { Choose(choice); });

    m_buttons[index] = button;

    return button;
}

void
IncomingCallScreen::SetCaller(const MSG::incoming_call& call)
{
    lv_label_set_text(m_name_label, call.caller_name.c_str());
    lv_label_set_text(m_number_label, call.caller_number.c_str());
}

void
IncomingCallScreen::OnActivation()
{
    Select(std::nullopt);
    m_exit_requested = false;
    m_exit_timer = m_parent.StartTimer(30s, [this]() {
        m_parent.HideIncomingCall();
        return std::nullopt;
    });
}

void
IncomingCallScreen::Update()
{
    if (m_exit_requested)
    {
        m_exit_requested = false;
        m_parent.EndIncomingCall();
    }
}

void
IncomingCallScreen::HandleInput(const Input::Event& event)
{
    // Touch is handled by the LVGL buttons, and the encoder button never opens the menu here
    switch (event.type)
    {
    case hal::IInput::EventType::kLeft:
        Select(Choice::kDecline);
        break;
    case hal::IInput::EventType::kRight:
        Select(Choice::kAccept);
        break;
    case hal::IInput::EventType::kButtonUp:
        // On release, so that a press already in progress when the call came is ignored
        if (m_selected)
        {
            Choose(*m_selected);
        }
        break;
    default:
        break;
    }
}

void
IncomingCallScreen::SetHelp(bool on [[maybe_unused]])
{
}

void
IncomingCallScreen::Select(std::optional<Choice> choice)
{
    m_selected = choice;

    for (auto i = 0u; i < m_buttons.size(); ++i)
    {
        lv_obj_set_state(
            m_buttons[i], LV_STATE_FOCUSED, choice && std::to_underlying(*choice) == i);
    }
}

void
IncomingCallScreen::Choose(Choice choice)
{
    if (choice == Choice::kAccept)
    {
        m_parent.m_post_office.Send<MSG::answer_call>();
    }
    else if (choice == Choice::kDecline)
    {
        m_parent.m_post_office.Send<MSG::decline_call>();
    }

    m_exit_requested = true;
}
