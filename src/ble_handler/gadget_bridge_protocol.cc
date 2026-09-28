#include "gadget_bridge_protocol.hh"

#include <cctype>
#include <cstdio>
#include <string>
#include <utility>

namespace
{

constexpr std::string_view kPrefix = "GB(";
constexpr std::string_view kSuffix = ")";

// From Gadgetbridge (BangleJSDeviceSupport, NavigationInfoSpec actions)
constexpr auto kStringToTurn = std::array {
    std::pair {"continue", TurnSymbol::kStraight},
    std::pair {"left", TurnSymbol::kTurnLeft},
    std::pair {"left_slight", TurnSymbol::kTurnSlightLeft},
    std::pair {"left_sharp", TurnSymbol::kTurnSharpLeft},
    std::pair {"right", TurnSymbol::kTurnRight},
    std::pair {"right_slight", TurnSymbol::kTurnSlightRight},
    std::pair {"right_sharp", TurnSymbol::kTurnSharpRight},
    std::pair {"keep_left", TurnSymbol::kForkLeft},
    std::pair {"keep_right", TurnSymbol::kForkRight},
    std::pair {"uturn_left", TurnSymbol::kUturnLeft},
    std::pair {"uturn_right", TurnSymbol::kUturnRight},
    std::pair {"offroute", TurnSymbol::kNone},
    std::pair {"roundabout_right", TurnSymbol::kRoundaboutRight},
    std::pair {"roundabout_left", TurnSymbol::kRoundaboutLeft},
    std::pair {"roundabout_straight", TurnSymbol::kRoundaboutStraight},
    // No roundabout U-turn symbol. Counter-clockwise (right-hand traffic), so a left U-turn
    std::pair {"roundabout_uturn", TurnSymbol::kUturnLeft},
    std::pair {"finish", TurnSymbol::kDestination},
    std::pair {"merge", TurnSymbol::kMerge},
};

TurnSymbol
StringToTurn(std::string_view s)
{
    for (const auto& [key, value] : kStringToTurn)
    {
        if (s == key)
        {
            return value;
        }
    }
    return TurnSymbol::kNone;
}

std::string_view
Trim(std::string_view s)
{
    // Gadgetbridge prefixes lines with \x10 (echo off), and sometimes sends \x03 (Ctrl-C)
    auto is_space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    auto is_junk = [&](char c) { return c == '\x10' || c == '\x03' || is_space(c); };

    while (!s.empty() && is_junk(s.front()))
    {
        s.remove_prefix(1);
    }
    while (!s.empty() && (is_space(s.back()) || s.back() == ';'))
    {
        s.remove_suffix(1);
    }

    return s;
}

// Gadgetbridge sends JavaScript, which can contain \xHH escapes that aren't valid JSON.
// Strings are also ISO-8859-1 (Espruino strings are 8-bit), so convert to UTF-8.
std::string
JsToJson(std::string_view s)
{
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); i++)
    {
        const auto c = static_cast<uint8_t>(s[i]);
        if (c >= 0x80)
        {
            out += static_cast<char>(0xc0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3f));
            continue;
        }

        if (s[i] == '\\' && i + 1 < s.size())
        {
            if (s[i + 1] == 'x')
            {
                out += "\\u00";
            }
            else
            {
                out += s[i];
                out += s[i + 1];
            }
            i++;
            continue;
        }

        out += s[i];
    }

    return out;
}

} // namespace

GadgetBridgeProtocol::GadgetBridgeProtocol(os::TimerManager& timer_manager, ApplicationState& state)
    : m_timer_manager(timer_manager)
    , m_state(state)
{
}

std::optional<nlohmann::json>
GadgetBridgeProtocol::ParseLine(std::string_view line)
{
    line = Trim(line);

    if (!line.starts_with(kPrefix) || !line.ends_with(kSuffix))
    {
        return std::nullopt;
    }
    line.remove_prefix(kPrefix.size());
    line.remove_suffix(kSuffix.size());

    // No exceptions on target, so use the non-throwing parser
    auto json = nlohmann::json::parse(JsToJson(line), nullptr, false);
    if (json.is_discarded())
    {
        return std::nullopt;
    }

    return json;
}

void
GadgetBridgeProtocol::SetSender(std::function<void(std::string_view)> sender)
{
    m_sender = std::move(sender);
}

void
GadgetBridgeProtocol::Send(const nlohmann::json& json)
{
    // Like the Bangle.js (Espruino println), send one JSON object per line. Gadgetbridge
    // unconditionally strips the character before the \n, so \r\n is required.
    auto str = json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\r\n";
    m_sender(str);
}

void
GadgetBridgeProtocol::PushLine(std::string_view line)
{
    auto json = ParseLine(line);

    if (!json)
    {
        printf("GB: ignoring line: %.*s\n", static_cast<int>(line.size()), line.data());
        return;
    }

    // Replace invalid UTF-8, since dump() would otherwise throw (i.e., abort)
    auto str = json->dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    printf("GB: %s\n", str.c_str());

    // Type
    if (!json->contains("t"))
    {
        return;
    }

    const auto& type = json->at("t");
    if (type == "nav")
    {
        HandleNavigationEvent(*json);
    }
    else if (type == "is_gps_active")
    {
        // Has GPS
        Send({{"t", "gps_power"},
              {"status", m_state.Get<AS::gps_status>() == GpsStatus::kPositionValid}});
    }
}

void
GadgetBridgeProtocol::HandleNavigationEvent(const nlohmann::json& json)
{
    /*
     * Something like
     *
     * {"action":"continue","distance":"0 m","eta":"06:46","instr":"towards Larsmässgatan","t":"nav"}
     * {"action":"right","distance":"160 m","eta":"06:46","instr":"Brittmässgatan","t":"nav"}
     *
     * (end navigation)
     * {"t":"nav"}
     */

    // Navigation cancelled immediately
    if (!json.contains("action"))
    {
        m_navigation_active_timer = nullptr;
        m_state.CheckoutReadWrite().Set<AS::navigation_active>(false);
        return;
    }

    auto distance = json.contains("distance") ? json.at("distance").get<std::string>() : "0";
    auto instr = json.contains("instr") ? json.at("instr").get<std::string>() : "";
    auto eta = json.contains("eta") ? json.at("eta").get<std::string>() : "";
    auto action = json.contains("action") ? json.at("action").get<std::string>() : "";

    auto qw = m_state.CheckoutQueuedWriter<AS::navigation_active,
                                           AS::next_street,
                                           AS::distance_to_next,
                                           AS::turn_symbol>();

    qw.Set<AS::navigation_active>(true);
    qw.Set<AS::next_street>(instr);
    qw.Set<AS::distance_to_next>(distance);
    qw.Set<AS::turn_symbol>(StringToTurn(action));

    // Long timeout for the case where the moped is stopped
    m_navigation_active_timer = m_timer_manager.StartTimer(1min, [this]() {
        auto rw = m_state.CheckoutReadWrite();

        rw.Set<AS::navigation_active>(false);

        return std::nullopt;
    });
    //qw.Set<AS::current_icon_hash>(action);
}
