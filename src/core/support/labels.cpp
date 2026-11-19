#include <deimos/support/labels.hpp>

#include <array>

namespace deimos::labels
{
namespace
{

template <typename Enum, std::size_t Size>
std::string_view lookup(const std::array<std::pair<Enum, const char*>, Size>& table, Enum value)
{
    for (const auto& entry : table)
    {
        if (entry.first == value)
        {
            return i18n::tr(entry.second);
        }
    }
    return {};
}

} // namespace

std::string_view severity(Severity value)
{
    static const std::array<std::pair<Severity, const char*>, 3> table{{
        {Severity::info, "severity.info"},
        {Severity::warning, "severity.warning"},
        {Severity::error, "severity.error"},
    }};
    return lookup(table, value);
}

std::string_view engineState(EngineState value)
{
    static const std::array<std::pair<EngineState, const char*>, 5> table{{
        {EngineState::stopped, "state.stopped"},
        {EngineState::starting, "state.starting"},
        {EngineState::running, "state.running"},
        {EngineState::stopping, "state.stopping"},
        {EngineState::faulted, "state.faulted"},
    }};
    return lookup(table, value);
}

std::string_view logLevel(LogLevel value)
{
    static const std::array<std::pair<LogLevel, const char*>, 5> table{{
        {LogLevel::trace, "level.trace"},
        {LogLevel::debug, "level.debug"},
        {LogLevel::info, "level.info"},
        {LogLevel::warning, "level.warning"},
        {LogLevel::error, "level.error"},
    }};
    return lookup(table, value);
}

std::string_view tool(ToolKind value)
{
    static const std::array<std::pair<ToolKind, const char*>, 4> table{{
        {ToolKind::autoDetect, "tool.auto"},
        {ToolKind::winws, "tool.winws"},
        {ToolKind::nfqws, "tool.nfqws"},
        {ToolKind::tpws, "tool.tpws"},
    }};
    return lookup(table, value);
}

std::string_view desyncMode(DesyncMode value)
{
    static const std::array<std::pair<DesyncMode, const char*>, 8> table{{
        {DesyncMode::split, "mode.split"},
        {DesyncMode::split2, "mode.split2"},
        {DesyncMode::disorder, "mode.disorder"},
        {DesyncMode::disorder2, "mode.disorder2"},
        {DesyncMode::fake, "mode.fake"},
        {DesyncMode::fakeSplit, "mode.fake_split"},
        {DesyncMode::fakeDisorder, "mode.fake_disorder"},
        {DesyncMode::hostFakeSplit, "mode.host_fake_split"},
    }};
    return lookup(table, value);
}

std::string_view fooling(Fooling value)
{
    static const std::array<std::pair<Fooling, const char*>, 6> table{{
        {Fooling::md5sig, "fooling.md5sig"},
        {Fooling::badseq, "fooling.badseq"},
        {Fooling::badsum, "fooling.badsum"},
        {Fooling::datanoack, "fooling.datanoack"},
        {Fooling::hopbyhop, "fooling.hopbyhop"},
        {Fooling::hopbyhop2, "fooling.hopbyhop2"},
    }};
    return lookup(table, value);
}

std::string_view l7(L7Protocol value)
{
    static const std::array<std::pair<L7Protocol, const char*>, 4> table{{
        {L7Protocol::tls, "l7.tls"},
        {L7Protocol::http, "l7.http"},
        {L7Protocol::quic, "l7.quic"},
        {L7Protocol::unknown, "l7.unknown"},
    }};
    return lookup(table, value);
}

std::string_view languageName(i18n::Language value)
{
    switch (value)
    {
    case i18n::Language::russian:
        return i18n::tr("lang.name.ru");
    case i18n::Language::english:
        return i18n::tr("lang.name.en");
    case i18n::Language::system:
        break;
    }
    return i18n::tr("lang.name.auto");
}

std::string duration(std::int64_t seconds)
{
    if (seconds <= 0)
    {
        return i18n::tr("time.seconds", {"0"});
    }

    const std::int64_t hours = seconds / 3600;
    const std::int64_t minutes = (seconds % 3600) / 60;
    const std::int64_t rest = seconds % 60;

    std::string text;
    if (hours > 0)
    {
        text += i18n::tr("time.hours", {std::to_string(hours)});
        text += " ";
    }
    if (hours > 0 || minutes > 0)
    {
        text += i18n::tr("time.minutes", {std::to_string(minutes)});
        text += " ";
    }
    text += i18n::tr("time.seconds", {std::to_string(rest)});
    return text;
}

std::string yesNo(bool value)
{
    return i18n::tr(value ? "common.yes" : "common.no");
}

}
