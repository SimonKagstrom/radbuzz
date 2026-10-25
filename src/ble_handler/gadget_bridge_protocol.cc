#include "gadget_bridge_protocol.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

namespace
{

constexpr std::string_view kPrefix = "GB(";
constexpr std::string_view kSuffix = ")";

constexpr std::string_view kSetTime = "setTime(";
constexpr std::string_view kSetTimeZone = "E.setTimeZone(";

// UTC-12 to UTC+14 (Kiribati)
constexpr auto kMinTimezoneMinutes = -12 * 60;
constexpr auto kMaxTimezoneMinutes = 14 * 60;

// Unit -> meters. Gadgetbridge passes on what the navigation app shows
constexpr auto kDistanceUnits = std::array {
    std::pair {std::string_view("m"), 1.0},
    std::pair {std::string_view("km"), 1000.0},
    std::pair {std::string_view("ft"), 0.3048},
    std::pair {std::string_view("yd"), 0.9144},
    std::pair {std::string_view("mi"), 1609.344},
};

// Spaces between the value and unit: regular, and (UTF-8) no-break and narrow no-break
constexpr auto kDistanceSpaces = std::array {
    std::string_view(" "),
    std::string_view("\xc2\xa0"),
    std::string_view("\xe2\x80\xaf"),
};

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

// The argument of e.g., "setTime(" up to the ")", or nullopt if unterminated
std::optional<std::string_view>
CallArgument(std::string_view s)
{
    auto end = s.find(')');
    if (end == std::string_view::npos)
    {
        return std::nullopt;
    }

    return s.substr(0, end);
}

// Hours like "2.0", "-3.5" or "5.75" (a Java float) to minutes, if valid
std::optional<int16_t>
HoursToMinutes(std::string_view hours)
{
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };

    const auto negative = hours.starts_with('-');
    if (negative)
    {
        hours.remove_prefix(1);
    }

    // By hand, since there's no std::from_chars for floats everywhere
    auto value = 0.0;
    auto digits = 0;
    while (!hours.empty() && is_digit(hours.front()) && digits < 3)
    {
        value = value * 10 + (hours.front() - '0');
        hours.remove_prefix(1);
        digits++;
    }
    if (hours.starts_with('.'))
    {
        hours.remove_prefix(1);
        for (auto scale = 0.1; !hours.empty() && is_digit(hours.front()); scale /= 10)
        {
            value += scale * (hours.front() - '0');
            hours.remove_prefix(1);
        }
    }

    if (digits == 0 || !hours.empty())
    {
        return std::nullopt;
    }

    const auto minutes = std::lround((negative ? -value : value) * 60);
    if (minutes < kMinTimezoneMinutes || minutes > kMaxTimezoneMinutes)
    {
        return std::nullopt;
    }

    return static_cast<int16_t>(minutes);
}

// A string field, or empty if missing or of another type (no exceptions on target)
std::string
GetString(const nlohmann::json& json, const char* key)
{
    return json.contains(key) && json.at(key).is_string() ? json.at(key).get<std::string>()
                                                          : std::string();
}

// The notification id (a Java int, so possibly negative), or nullopt if missing
std::optional<uint32_t>
GetNotificationId(const nlohmann::json& json)
{
    if (!json.contains("id") || !json.at("id").is_number_integer())
    {
        return std::nullopt;
    }

    return static_cast<uint32_t>(json.at("id").get<int64_t>());
}

} // namespace

GadgetBridgeProtocol::GadgetBridgeProtocol(os::TimerManager& timer_manager,
                                           ApplicationState& state,
                                           PostOffice<MSG::AllMessages>& post_office,
                                           IEventNotifier& notifier)
    : m_timer_manager(timer_manager)
    , m_state(state)
    , m_post_office(post_office)
    , m_state_listener(m_state.AttachListener<AS::battery_soc, AS::bluetooth_connected>(notifier))
    , m_state_cache(m_state)
{
    m_mailbox = m_post_office.Subscribe<MSG::answer_call,
                                        MSG::decline_call,
                                        MSG::hangup_call,
                                        MSG::user_dismissed_message>(notifier);
}

void
GadgetBridgeProtocol::SendCallControl(std::string_view command)
{
    // Gadgetbridge maps "n" (case insensitive) to GBDeviceEventCallControl.Event
    Send({{"t", "call"}, {"n", command}});
}

void
GadgetBridgeProtocol::Poll()
{
    // On changes, and on connect since the phone doesn't remember it
    auto& co = m_state_cache.Pull();

    co.OnNewValue<AS::bluetooth_connected>([this, &co](auto connected) {
          if (connected)
          {
              Send({{"t", "status"}, {"bat", co.Get<AS::battery_soc>()}});
          }
      }).OnNewValue<AS::battery_soc>([this, &co](auto soc) {
        if (co.Get<AS::bluetooth_connected>())
        {
            Send({{"t", "status"}, {"bat", soc}});
        }
    });


    m_mailbox->Collect()
        .On<MSG::answer_call>([this]() { SendCallControl("accept"); })
        .On<MSG::decline_call>([this]() {
            SendCallControl("reject");
            m_state.Set<AS::call_ongoing>(false);
        })
        .On<MSG::hangup_call>([this]() {
            SendCallControl("end");
            m_state.Set<AS::call_ongoing>(false);
        })
        .On<MSG::user_dismissed_message>([this](auto msg) {
            // Back to a Java int, which Gadgetbridge maps to its notification
            Send({{"t", "notify"}, {"id", static_cast<int32_t>(msg->id)}, {"n", "DISMISS"}});
        });
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

std::optional<MSG::network_time>
GadgetBridgeProtocol::ParseSetTime(std::string_view line)
{
    line = Trim(line);
    if (!line.starts_with(kSetTime))
    {
        return std::nullopt;
    }
    line.remove_prefix(kSetTime.size());

    // Seconds since the epoch (UTC). Rejects signs, decimals and values not fitting a uint32_t
    auto utc_string = CallArgument(line);
    if (!utc_string)
    {
        return std::nullopt;
    }

    uint32_t utc = 0;
    auto [ptr, ec] =
        std::from_chars(utc_string->data(), utc_string->data() + utc_string->size(), utc);
    if (ec != std::errc() || ptr != utc_string->data() + utc_string->size())
    {
        return std::nullopt;
    }

    MSG::network_time out {utc, std::nullopt};

    // The current offset in hours, so including DST. Optional, and the time is fine without it
    if (auto pos = line.find(kSetTimeZone); pos != std::string_view::npos)
    {
        if (auto hours = CallArgument(line.substr(pos + kSetTimeZone.size())))
        {
            out.timezone_minutes = HoursToMinutes(*hours);
        }
    }

    return out;
}

uint32_t
GadgetBridgeProtocol::DistanceToMeters(std::string_view distance)
{
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    auto skip_spaces = [&distance]() {
        for (auto again = true; again;)
        {
            again = false;
            for (auto space : kDistanceSpaces)
            {
                if (distance.starts_with(space))
                {
                    distance.remove_prefix(space.size());
                    again = true;
                }
            }
        }
    };

    skip_spaces();

    // No std::from_chars for floats everywhere, so parse by hand. Both . and , as decimal
    // separator (apps switch to the larger unit before thousands separators are needed)
    auto value = 0.0;
    auto digits = 0;
    while (!distance.empty() && is_digit(distance.front()))
    {
        value = value * 10 + (distance.front() - '0');
        distance.remove_prefix(1);
        digits++;
    }
    if (!distance.empty() && (distance.front() == '.' || distance.front() == ',') &&
        distance.size() > 1 && is_digit(distance[1]))
    {
        distance.remove_prefix(1);
        for (auto scale = 0.1; !distance.empty() && is_digit(distance.front()); scale /= 10)
        {
            value += scale * (distance.front() - '0');
            distance.remove_prefix(1);
            digits++;
        }
    }

    // Also guards against overflow below
    if (digits == 0 || digits > 9)
    {
        return 0;
    }

    skip_spaces();
    while (!distance.empty() && std::isspace(static_cast<unsigned char>(distance.back())))
    {
        distance.remove_suffix(1);
    }

    // Plain numbers are meters
    if (distance.empty())
    {
        return static_cast<uint32_t>(std::lround(value));
    }

    for (const auto& [unit, factor] : kDistanceUnits)
    {
        auto matches = unit.size() == distance.size() &&
                       std::equal(unit.begin(), unit.end(), distance.begin(), [](char a, char b) {
                           return a == std::tolower(static_cast<unsigned char>(b));
                       });
        if (matches)
        {
            return static_cast<uint32_t>(std::lround(value * factor));
        }
    }

    return 0;
}

uint32_t
GadgetBridgeProtocol::RoundDistance(uint32_t meters)
{
    if (meters == 0)
    {
        return 0;
    }

    const uint32_t step = meters < 100 ? 5u : 10u;
    const auto rounded = (static_cast<uint64_t>(meters) + step / 2) / step * step;

    // Round down instead if rounding up would overflow
    if (rounded > std::numeric_limits<uint32_t>::max())
    {
        return meters / step * step;
    }

    return std::max(static_cast<uint32_t>(rounded), step);
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
    if (auto time = ParseSetTime(line))
    {
        printf("GB: time %u, timezone %d minutes\n",
               static_cast<unsigned>(time->utc),
               time->timezone_minutes.value_or(0));
        m_post_office.Send<MSG::network_time>(*time);
        return;
    }

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
    else if (type == "call")
    {
        HandleCallEvent(*json);
    }
    else if (type == "notify")
    {
        HandleNotification(*json);
    }
    else if (type == "notify-")
    {
        HandleNotificationRemoval(*json);
    }
    else if (type == "is_gps_active")
    {
        // Has GPS
        Send({{"t", "gps_power"},
              {"status", m_state.Get<AS::gps_status>() == GpsStatus::kPositionValid}});
    }
}

void
GadgetBridgeProtocol::HandleCallEvent(const nlohmann::json& json)
{
    /*
     * The cmd is the CallSpec command, so something like
     *
     * {"t":"call","cmd":"incoming","name":"Gregor Samsa","number":"+46701234567"}
     * {"t":"call","cmd":"start","name":"","number":""}   (answered)
     * {"t":"call","cmd":"outgoing","name":"Gregor Samsa","number":"+46701234567"}
     * {"t":"call","cmd":"end","name":"","number":""}     (hung up, rejected or missed)
     *
     * cmd can also be "accept", "reject" or "undefined".
     */
    if (!json.contains("cmd") || !json.at("cmd").is_string())
    {
        return;
    }

    const auto cmd = GetString(json, "cmd");

    if (cmd == "incoming")
    {
        m_post_office.Send<MSG::incoming_call>(
            {GetString(json, "number"), GetString(json, "name")});
        m_state.Set<AS::call_ongoing>(true);
    }
    else if (cmd == "start")
    {
        // Answered, possibly on the phone
        m_post_office.Send<MSG::call_started>();
        m_state.Set<AS::call_ongoing>(true);
    }
    else if (cmd == "outgoing")
    {
        // Made from the phone, so nothing to show here. Not followed by "start" when answered
        m_state.Set<AS::call_ongoing>(true);
    }
    else if (cmd == "end" || cmd == "reject")
    {
        m_post_office.Send<MSG::call_ended>();
        m_state.Set<AS::call_ongoing>(false);
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
        m_state.Set<AS::navigation_active>(false);
        return;
    }

    auto distance = RoundDistance(DistanceToMeters(GetString(json, "distance")));
    auto instr = GetString(json, "instr");
    auto action = GetString(json, "action");

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
        m_state.Set<AS::navigation_active>(false);

        return std::nullopt;
    });
}

void
GadgetBridgeProtocol::HandleNotification(const nlohmann::json& json)
{
    /*
     * Something like (the text is cut by Gadgetbridge)
     *
     * {"t":"notify","id":1790422616,"src":"Gmail","title":"Prisjakt","subject":"","sender":"",
     *  "body":"Logga in för att behålla ditt konto\n...","reply":true,
     *  "actions":[{"title":"Archive"},{"title":"Delete"},{"title":"Mark as read"}]}
     *
     * For SMS, src is "SMS Message", the title the contact and sender/tel the number.
     */
    const auto id = GetNotificationId(json);
    if (!id)
    {
        return;
    }

    auto title = GetString(json, "title");
    if (title.empty())
    {
        title = GetString(json, "sender");
    }

    // E.g., an e-mail subject, which goes first in the text
    auto body = GetString(json, "body");
    if (auto subject = GetString(json, "subject"); !subject.empty())
    {
        body = body.empty() ? subject : subject + "\n" + body;
    }

    m_post_office.Send<MSG::message>({*id, GetString(json, "src"), title, body});
}

void
GadgetBridgeProtocol::HandleNotificationRemoval(const nlohmann::json& json)
{
    // Dismissed on the phone: {"t":"notify-","id":1790422616}
    if (const auto id = GetNotificationId(json))
    {
        m_post_office.Send<MSG::dismiss_message>({*id});
    }
}
