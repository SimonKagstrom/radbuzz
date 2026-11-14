#include "navigation_widget.hh"

#include "distance_string.hh"
#include "ui_style.hh"

#include <array>
#include <format>
#include <radbuzz_turn_symbols_60.h>

namespace
{

consteval auto
ToUtf8(uint32_t code_point)
{
    // Null-terminated, for use as a C string
    std::array<char, 4> out {};

    out[0] = static_cast<char>(0xE0 | (code_point >> 12));
    out[1] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (code_point & 0x3F));

    return out;
}
constexpr auto kTurnSymbolStrings = std::array {
    std::array<char, 4> {'\0'},
    ToUtf8(0xe0c8), // kDestination
    ToUtf8(0xeb95), // kStraight
    ToUtf8(0xebab), // kTurnRight
    ToUtf8(0xeba6), // kTurnLeft
    ToUtf8(0xebaa), // kTurnSharpRight
    ToUtf8(0xeba7), // kTurnSharpLeft
    ToUtf8(0xeb9a), // kTurnSlightRight
    ToUtf8(0xeba4), // kTurnSlightLeft
    ToUtf8(0xeba2), // kUturnRight
    ToUtf8(0xeba1), // kUturnLeft
    ToUtf8(0xeb96), // kRampRight
    ToUtf8(0xeb9c), // kRampLeft
    ToUtf8(0xebac), // kForkRight
    ToUtf8(0xeba0), // kForkLeft
    ToUtf8(0xeb95), // kRoundaboutStraight (same as straight)
    ToUtf8(0xeba3), // kRoundaboutRight
    ToUtf8(0xeb99), // kRoundaboutLeft
    ToUtf8(0xeb98), // kMerge
};

static_assert(kTurnSymbolStrings.size() == static_cast<size_t>(TurnSymbol::kValueCount));

} // namespace

NavigationWidget::NavigationWidget(lv_obj_t* parent)
{
    // Left pane
    // Clips the navigation box, so that its left corners are hidden
    m_left_box = lv_obj_create(parent);
    lv_obj_set_size(m_left_box, 128 + 10, hal::kDisplayHeight + 10);
    lv_obj_set_style_bg_opa(m_left_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_left_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_left_box, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(m_left_box, false);

    // Push left rounded corners off-screen for the navigation pane while keeping right corners.
    constexpr int kLeftCornerClipPx = 16;
    constexpr int kPaneCornerRadius = 18;

    // Navigation
    m_navigation_box = ui::CreatePane(m_left_box, kPaneCornerRadius);
    lv_obj_set_size(m_navigation_box, 128 + kLeftCornerClipPx, 128);
    lv_obj_align(m_navigation_box, LV_ALIGN_BOTTOM_LEFT, -32, 32);

    m_current_turn_symbol = ui::CreateLabel(m_navigation_box, &radbuzz_turn_symbols_60);
    lv_obj_align(m_current_turn_symbol, LV_ALIGN_TOP_MID, 8, 0);

    m_distance_left_label = ui::CreateLabel(m_navigation_box);
    lv_obj_align(m_distance_left_label, LV_ALIGN_BOTTOM_MID, 0, 2);

    // Positioned and sized in SetLeftX()
    m_navigation_description_box = ui::CreatePane(parent);

    m_description_label = ui::CreateLabel(m_navigation_description_box);
    lv_obj_align(m_description_label, LV_ALIGN_TOP_LEFT, 4, -18);

    SetLeftX(0);
}

void
NavigationWidget::SetLeftX(int32_t left_x)
{
    m_left_x = left_x;
    lv_obj_align(m_left_box, LV_ALIGN_TOP_LEFT, left_x - 10, -10);

    // To the right edge (the power bar ends above it)
    const auto description_x = left_x + 128 - kDescriptionBoxHeight;
    lv_obj_align(m_navigation_description_box, LV_ALIGN_BOTTOM_LEFT, description_x, 0);
    lv_obj_set_size(
        m_navigation_description_box, hal::kDisplayWidth - description_x, kDescriptionBoxHeight);
}

void
NavigationWidget::Update(ApplicationState& state, bool show, int32_t left_x)
{
    if (left_x != m_left_x)
    {
        SetLeftX(left_x);
    }

    auto navigation_active = show && state.Get<AS::navigation_active>();

    lv_obj_set_hidden(m_navigation_box, !navigation_active);
    lv_obj_set_hidden(m_navigation_description_box, !navigation_active);

    if (navigation_active)
    {
        lv_label_set_text(m_description_label,
                          std::format("{}", *state.Get<AS::next_street>()).c_str());
        lv_label_set_text(m_distance_left_label,
                          DistanceToString(state.Get<AS::distance_to_next>()).c_str());

        auto turn = std::to_underlying(state.Get<AS::turn_symbol>());
        if (turn < kTurnSymbolStrings.size())
        {
            lv_label_set_text(m_current_turn_symbol, kTurnSymbolStrings[turn].data());
        }
    }
}
