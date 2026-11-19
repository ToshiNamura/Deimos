#include <deimos/model/profile.hpp>

#include <deimos/model/diagnostic_i18n.hpp>

#include <algorithm>

#include <deimos/support/strings.hpp>

#include "model_json.hpp"
#include "profile_internal.hpp"

namespace deimos
{
namespace
{

DesyncRound parseRound(const detail::Json& node, std::string_view path, std::vector<Diagnostic>& diagnostics)
{
    DesyncRound round;
    if (!node.is_object())
    {
        diagnostics.push_back(errorAt("profile.round.invalid", std::string{path}));
        return round;
    }

    if (const auto filter = node.find("filter"); filter != node.end() && filter->is_object())
    {
        round.filter.tcpPorts = detail::readPorts(*filter, "tcp", std::string{path} + ".filter", diagnostics);
        round.filter.udpPorts = detail::readPorts(*filter, "udp", std::string{path} + ".filter", diagnostics);
        round.filter.l7 = detail::readEnumArray<L7Protocol>(*filter,
                                                            "l7",
                                                            &parseL7Protocol,
                                                            std::string{path} + ".filter",
                                                            diagnostics);
    }

    round.modes = detail::readEnumArray<DesyncMode>(node, "modes", &parseDesyncMode, path, diagnostics);
    round.fooling = detail::readEnumArray<Fooling>(node, "fooling", &parseFooling, path, diagnostics);

    round.splitPos = detail::readOptionalInt(node, "splitPos");
    round.splitSeqOverlap = detail::readOptionalInt(node, "splitSeqOverlap");
    round.ttl = detail::readOptionalInt(node, "ttl");
    round.autoTtl = detail::readOptionalInt(node, "autoTtl");
    round.repeats = detail::readOptionalInt(node, "repeats");
    round.windowSize = detail::readOptionalInt(node, "windowSize");

    if (const auto payload = node.find("fakePayload"); payload != node.end() && payload->is_object())
    {
        FakePayload parsed;
        const std::string kindText = payload->value("kind", std::string{"file"});
        if (!parseFakePayloadKind(kindText, parsed.kind))
        {
            diagnostics.push_back(
                errorAt("profile.fake_payload.invalid_kind", std::string{path} + ".fakePayload.kind"));
        }
        parsed.value = payload->value("value", std::string{});
        round.fakePayload = std::move(parsed);
    }

    round.hostlists = detail::readStringArray(node, "hostlists");
    round.hostlistExcludes = detail::readStringArray(node, "hostlistExcludes");
    round.ipsets = detail::readStringArray(node, "ipsets");
    return round;
}

detail::Json writeRound(const DesyncRound& round)
{
    detail::Json node = detail::Json::object();

    detail::Json filter = detail::Json::object();
    detail::writePorts(filter, "tcp", round.filter.tcpPorts);
    detail::writePorts(filter, "udp", round.filter.udpPorts);
    if (!round.filter.l7.empty())
    {
        filter["l7"] = detail::writeEnumArray(round.filter.l7, &toCliToken);
    }
    if (!filter.empty())
    {
        node["filter"] = std::move(filter);
    }

    if (!round.modes.empty())
    {
        node["modes"] = detail::writeEnumArray(round.modes, &toCliToken);
    }
    detail::writeOptionalInt(node, "splitPos", round.splitPos);
    detail::writeOptionalInt(node, "splitSeqOverlap", round.splitSeqOverlap);
    if (!round.fooling.empty())
    {
        node["fooling"] = detail::writeEnumArray(round.fooling, &toCliToken);
    }
    detail::writeOptionalInt(node, "ttl", round.ttl);
    detail::writeOptionalInt(node, "autoTtl", round.autoTtl);
    detail::writeOptionalInt(node, "repeats", round.repeats);
    detail::writeOptionalInt(node, "windowSize", round.windowSize);

    if (round.fakePayload.has_value())
    {
        node["fakePayload"] = detail::Json{
            {"kind", std::string{toCliToken(round.fakePayload->kind)}},
            {"value", round.fakePayload->value},
        };
    }

    detail::writeStringArray(node, "hostlists", round.hostlists);
    detail::writeStringArray(node, "hostlistExcludes", round.hostlistExcludes);
    detail::writeStringArray(node, "ipsets", round.ipsets);
    return node;
}

bool isValidIdCharacter(char character)
{
    return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '-' ||
           character == '_' || character == '.';
}

} // namespace

std::string slugify(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool previousDash = false;
    for (const char rawCharacter : text)
    {
        const char character = static_cast<char>(std::tolower(static_cast<unsigned char>(rawCharacter)));
        const bool isAlphaNumeric = (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9');
        if (isAlphaNumeric)
        {
            result.push_back(character);
            previousDash = false;
            continue;
        }
        if (!result.empty() && !previousDash)
        {
            result.push_back('-');
            previousDash = true;
        }
    }
    while (!result.empty() && result.back() == '-')
    {
        result.pop_back();
    }
    if (result.size() > 64)
    {
        result.resize(64);
    }
    return result;
}

std::vector<Diagnostic> validateProfile(const Profile& profile, std::size_t index)
{
    std::vector<Diagnostic> diagnostics;
    const std::string base = "profiles[" + std::to_string(index) + "]";

    if (profile.id.empty())
    {
        diagnostics.push_back(errorAt("profile.id.empty", base + ".id"));
    }
    else
    {
        const bool validCharacters =
            std::all_of(profile.id.begin(), profile.id.end(), isValidIdCharacter) && profile.id.front() != '-' &&
            profile.id.front() != '.';
        if (!validCharacters)
        {
            diagnostics.push_back(errorAt("profile.id.invalid", base + ".id"));
        }
    }

    if (profile.name.empty())
    {
        diagnostics.push_back(warningAt("profile.name.empty", base + ".name"));
    }

    if (profile.rounds.empty())
    {
        diagnostics.push_back(errorAt("profile.rounds.empty", base + ".rounds"));
    }

    if (profile.nfqueue == 0)
    {
        diagnostics.push_back(errorAt("profile.nfqueue.invalid", base + ".nfqueue"));
    }

    for (std::size_t roundIndex = 0; roundIndex < profile.rounds.size(); ++roundIndex)
    {
        const std::string roundPath = base + ".rounds[" + std::to_string(roundIndex) + "]";
        const auto roundDiagnostics = validateRound(profile.rounds[roundIndex], roundPath, profile.tool);
        diagnostics.insert(diagnostics.end(), roundDiagnostics.begin(), roundDiagnostics.end());

        if (profile.tool != ToolKind::tpws && profile.rounds[roundIndex].filter.l7.size() > 0 &&
            profile.rounds[roundIndex].filter.isEmpty())
        {
            diagnostics.push_back(warningAt("profile.round.l7_without_ports", roundPath + ".filter.l7"));
        }
    }

    for (const std::string& argument : profile.extraArguments)
    {
        if (argument == "--new")
        {
            diagnostics.push_back(errorAt("profile.extra_arguments.reserved", base + ".extraArguments"));
        }
    }

    return diagnostics;
}

std::string serializeProfile(const Profile& profile, bool pretty)
{
    const detail::Json node = detail::writeProfileJson(profile);
    return node.dump(pretty ? 2 : -1);
}

Status parseProfile(std::string_view text, Profile& out, std::vector<Diagnostic>& diagnostics)
{
    detail::Json root;
    try
    {
        root = detail::Json::parse(text);
    }
    catch (const std::exception& exception)
    {
        diagnostics.push_back(errorAt("profile.invalid_json", "", {exception.what()}));
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.profile.invalid_json"), exception.what());
    }

    if (!root.is_object())
    {
        diagnostics.push_back(errorAt("profile.invalid_json.object", ""));
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.profile.not_an_object"));
    }

    Profile parsed;
    try
    {
        parsed = detail::parseProfileJson(root, "profile", diagnostics);
    }
    catch (const std::exception& exception)
    {
        diagnostics.push_back(errorAt("profile.invalid_type", "", {exception.what()}));
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.profile.wrong_type"), exception.what());
    }

    const auto profileDiagnostics = validateProfile(parsed, 0);
    diagnostics.insert(diagnostics.end(), profileDiagnostics.begin(), profileDiagnostics.end());

    out = std::move(parsed);
    if (hasErrors(diagnostics))
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.profile.invalid"));
    }
    return Status::ok();
}

ProfileSummary summarize(const Profile& profile)
{
    ProfileSummary summary;
    summary.id = profile.id;
    summary.name = profile.name;
    summary.tool = profile.tool;
    summary.enabled = profile.enabled;
    summary.roundCount = profile.rounds.size();
    summary.description = profile.description;
    return summary;
}

std::string serializeProfileSummary(const ProfileSummary& summary, bool pretty)
{
    const detail::Json node{
        {"id", summary.id},
        {"name", summary.name},
        {"tool", std::string{toCliToken(summary.tool)}},
        {"enabled", summary.enabled},
        {"rounds", summary.roundCount},
        {"description", summary.description},
    };
    return node.dump(pretty ? 2 : -1);
}

std::string serializeProfileSummaries(const std::vector<ProfileSummary>& summaries, bool pretty)
{
    detail::Json array = detail::Json::array();
    for (const ProfileSummary& summary : summaries)
    {
        array.push_back(detail::Json::parse(serializeProfileSummary(summary, false)));
    }
    return array.dump(pretty ? 2 : -1);
}

namespace detail
{

Profile parseProfileJson(const Json& node, std::string_view path, std::vector<Diagnostic>& diagnostics)
{
    Profile profile;
    const std::string base{path};

    profile.id = node.value("id", std::string{});
    profile.name = node.value("name", std::string{});
    profile.description = node.value("description", std::string{});
    profile.notes = node.value("notes", std::string{});
    profile.enabled = node.value("enabled", true);

    const std::string toolText = node.value("tool", std::string{"auto"});
    if (!parseToolKind(toolText, profile.tool))
    {
        diagnostics.push_back(errorAt("profile.tool.unknown", jsonPath(base, "tool"), {toolText}));
    }

    const int nfqueue = node.value("nfqueue", 200);
    if (nfqueue > 0 && nfqueue <= 65535)
    {
        profile.nfqueue = static_cast<std::uint16_t>(nfqueue);
    }
    else
    {
        diagnostics.push_back(errorAt("profile.nfqueue.range", jsonPath(base, "nfqueue")));
    }

    profile.winDivertTcpPorts = readPorts(node, "winDivertTcpPorts", base, diagnostics);
    profile.winDivertUdpPorts = readPorts(node, "winDivertUdpPorts", base, diagnostics);

    if (const auto rounds = node.find("rounds"); rounds != node.end() && rounds->is_array())
    {
        std::size_t roundIndex = 0;
        for (const Json& roundNode : *rounds)
        {
            const std::string roundPath = base + ".rounds[" + std::to_string(roundIndex) + "]";
            profile.rounds.push_back(parseRound(roundNode, roundPath, diagnostics));
            ++roundIndex;
        }
    }

    profile.hostlists = readStringArray(node, "hostlists");
    profile.hostlistExcludes = readStringArray(node, "hostlistExcludes");
    profile.ipsets = readStringArray(node, "ipsets");
    profile.extraArguments = readStringArray(node, "extraArguments");
    return profile;
}

Json writeProfileJson(const Profile& profile)
{
    Json node = Json::object();
    node["id"] = profile.id;
    node["name"] = profile.name;
    if (!profile.description.empty())
    {
        node["description"] = profile.description;
    }
    if (!profile.notes.empty())
    {
        node["notes"] = profile.notes;
    }
    node["tool"] = std::string{toCliToken(profile.tool)};
    node["nfqueue"] = profile.nfqueue;
    writePorts(node, "winDivertTcpPorts", profile.winDivertTcpPorts);
    writePorts(node, "winDivertUdpPorts", profile.winDivertUdpPorts);

    Json rounds = Json::array();
    for (const DesyncRound& round : profile.rounds)
    {
        rounds.push_back(writeRound(round));
    }
    node["rounds"] = std::move(rounds);

    writeStringArray(node, "hostlists", profile.hostlists);
    writeStringArray(node, "hostlistExcludes", profile.hostlistExcludes);
    writeStringArray(node, "ipsets", profile.ipsets);
    writeStringArray(node, "extraArguments", profile.extraArguments);
    node["enabled"] = profile.enabled;
    return node;
}

} // namespace detail

}
