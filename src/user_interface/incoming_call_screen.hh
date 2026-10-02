#pragma once

#include "messages.hh"
#include "user_interface.hh"

#include <array>
#include <optional>

class IncomingCallScreen : public UserInterface::ScreenBase
{
public:
    explicit IncomingCallScreen(UserInterface& parent);

    void SetCaller(const MSG::incoming_call& call);

private:
    // Left to right
    enum class Choice
    {
        kDecline,
        kAccept,

        kValueCount,
    };

    void OnActivation() final;
    void Update() final;
    void HandleInput(const Input::Event& event) final;
    void SetHelp(bool on) final;

    lv_obj_t* CreateButton(Choice choice, const char* text, lv_color_t color);
    void Select(std::optional<Choice> choice);
    void Choose(Choice choice);

    lv_obj_t* m_phone_icon_label {nullptr};
    lv_obj_t* m_name_label {nullptr};
    lv_obj_t* m_number_label {nullptr};
    std::array<lv_obj_t*, std::to_underlying(Choice::kValueCount)> m_buttons {};

    // Nothing is selected until the encoder is turned
    std::optional<Choice> m_selected;
    // Leave the screen on the next Update, so that the input isn't passed on to the next screen
    bool m_exit_requested {false};
};
