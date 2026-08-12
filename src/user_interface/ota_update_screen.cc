#include "ota_update_screen.hh"


namespace
{

constexpr std::string_view kProgressText =
    "Als Gregor Samsa eines Morgens aus unruhigen Traumen erwachte, fand er sich in seinem Bett zu "
    "einem ungeheuren Ungeziefer verwandelt";

constexpr auto kNumberOfWords = std::ranges::count(kProgressText, ' ') + 1;
static_assert(kNumberOfWords < 100);

auto
ProgressToString(uint8_t progress)
{
    if (progress == 100)
    {
        return std::string(kProgressText);
    }

    auto words = (((progress * 0xff) / 100) * kNumberOfWords) / 0xff;

    ssize_t offset = 0;
    for (auto word = 0; word < words; word++)
    {
        offset += kProgressText.substr(offset).find(' ') + 1;
    }

    return std::string(kProgressText.substr(0, offset));
}

} // namespace

OtaUpdateScreen::OtaUpdateScreen(UserInterface& parent)
    : UserInterface::ScreenBase(parent, lv_obj_create(nullptr))
{
    auto instructions = parent.m_ota_updater.GetInstructions();
    m_label = lv_label_create(m_screen);

    lv_obj_set_style_text_font(m_label, &lv_font_montserrat_22, LV_PART_MAIN);

    lv_label_set_text(m_label, instructions);

    lv_obj_set_style_text_align(m_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(m_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(m_label, hal::kDisplayWidth * 0.7f);
    lv_label_set_long_mode(m_label, LV_LABEL_LONG_WRAP);

    m_progress_cookie = parent.m_ota_updater.SubscribeOnProgress([this, &parent](auto progress) {
        m_progress = progress;
        parent.Awake();
    });
}

void
OtaUpdateScreen::Update()
{
    auto progress_now = m_progress.load();

    if (progress_now > 0)
    {
        auto text = ProgressToString(progress_now);

        lv_label_set_text(m_label, text.c_str());
    }
}
