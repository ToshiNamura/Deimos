#include <deimos/zapret/command_builder.hpp>

#include <algorithm>
#include <utility>

#include <deimos/model/diagnostic_i18n.hpp>
#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

bool containsProtocol(const std::vector<L7Protocol>& protocols, L7Protocol value)
{
    return std::find(protocols.begin(), protocols.end(), value) != protocols.end();
}

bool containsMode(const std::vector<DesyncMode>& modes, DesyncMode value)
{
    return std::find(modes.begin(), modes.end(), value) != modes.end();
}

void appendUniquePorts(std::vector<std::uint16_t>& target, const std::vector<std::uint16_t>& source)
{
    for (const std::uint16_t port : source)
    {
        if (std::find(target.begin(), target.end(), port) == target.end())
        {
            target.push_back(port);
        }
    }
}

std::string payloadValue(const FakePayload& payload)
{
    if (payload.kind == FakePayloadKind::hex)
    {
        return "@0x" + payload.value;
    }
    return payload.value;
}

std::string fakeOptionFor(const DesyncRound& round)
{
    if (containsProtocol(round.filter.l7, L7Protocol::quic))
    {
        return "--dpi-desync-fake-quic";
    }
    if (containsProtocol(round.filter.l7, L7Protocol::http))
    {
        return "--dpi-desync-fake-http";
    }
    if (containsProtocol(round.filter.l7, L7Protocol::tls))
    {
        return "--dpi-desync-fake-tls";
    }
    if (round.filter.tcpPorts.empty() && !round.filter.udpPorts.empty())
    {
        return "--dpi-desync-fake-quic";
    }
    return "--dpi-desync-fake-tls";
}

bool referencedFileExists(const std::string& value, const ToolPaths& paths, std::string_view workingDirectory)
{
    std::error_code code;
    const std::filesystem::path candidate{value};
    if (candidate.is_absolute())
    {
        return std::filesystem::exists(candidate, code);
    }

    std::vector<std::filesystem::path> bases;
    if (!workingDirectory.empty())
    {
        bases.emplace_back(std::string{workingDirectory});
    }
    if (!paths.listsDirectory.empty())
    {
        bases.push_back(paths.listsDirectory);
    }
    if (!paths.baseDirectory.empty())
    {
        bases.push_back(paths.baseDirectory);
    }
    bases.emplace_back("lists");
    bases.emplace_back(".");

    for (const auto& base : bases)
    {
        if (std::filesystem::exists(base / candidate, code))
        {
            return true;
        }
    }
    return false;
}

class ArgumentList
{
public:
    explicit ArgumentList(std::vector<std::string>& arguments)
        : arguments_(arguments)
    {
    }

    void flag(std::string_view name)
    {
        arguments_.emplace_back(name);
    }

    void option(std::string_view name, std::string_view value)
    {
        arguments_.emplace_back(std::string{name} + "=" + std::string{value});
    }

    void option(std::string_view name, int value)
    {
        option(name, std::to_string(value));
    }

private:
    std::vector<std::string>& arguments_;
};

void appendListArguments(ArgumentList& arguments,
                         const Profile& profile,
                         const DesyncRound& round,
                         const ToolPaths& paths,
                         std::string_view workingDirectory,
                         std::string_view roundPath,
                         bool isTpws,
                         std::vector<Diagnostic>& diagnostics)
{
    const auto appendLists = [&](const std::vector<std::string>& profileLists,
                                 const std::vector<std::string>& roundLists,
                                 std::string_view option,
                                 const char* field)
    {
        std::vector<std::string> merged;
        for (const std::string& entry : profileLists)
        {
            if (!entry.empty() && std::find(merged.begin(), merged.end(), entry) == merged.end())
            {
                merged.push_back(entry);
            }
        }
        for (const std::string& entry : roundLists)
        {
            if (!entry.empty() && std::find(merged.begin(), merged.end(), entry) == merged.end())
            {
                merged.push_back(entry);
            }
        }

        for (const std::string& entry : merged)
        {
            if (paths.any() && !referencedFileExists(entry, paths, workingDirectory))
            {
                diagnostics.push_back(warningAt("zapret.list.missing",
                                                std::string{roundPath} + "." + field,
                                                {std::string{field}, entry}));
            }
            arguments.option(option, entry);
        }
    };

    appendLists(profile.hostlists, round.hostlists, isTpws ? "--hosts" : "--hostlist", "hostlists");
    appendLists(profile.hostlistExcludes, round.hostlistExcludes, "--hostlist-exclude", "hostlistExcludes");
    appendLists(profile.ipsets, round.ipsets, isTpws ? "--ipset" : "--ipset", "ipsets");
}

void appendDesyncArguments(ArgumentList& arguments,
                           const DesyncRound& round,
                           std::string_view roundPath,
                           std::vector<Diagnostic>& diagnostics)
{
    if (!round.modes.empty())
    {
        arguments.option("--dpi-desync", serializeDesyncModes(round.modes));
    }
    if (round.splitPos.has_value())
    {
        arguments.option("--dpi-desync-split-pos", *round.splitPos);
    }
    if (round.splitSeqOverlap.has_value())
    {
        arguments.option("--dpi-desync-split-seqovl", *round.splitSeqOverlap);
    }
    if (!round.fooling.empty())
    {
        arguments.option("--dpi-desync-fooling", serializeFooling(round.fooling));
    }
    if (round.ttl.has_value())
    {
        arguments.option("--dpi-desync-ttl", *round.ttl);
    }
    if (round.autoTtl.has_value())
    {
        arguments.option("--dpi-desync-autottl", *round.autoTtl);
    }
    if (round.repeats.has_value())
    {
        arguments.option("--dpi-desync-repeats", *round.repeats);
    }
    if (round.windowSize.has_value())
    {
        arguments.option("--wssize", *round.windowSize);
    }
    if (round.fakePayload.has_value() && round.usesFakePayload())
    {
        if (round.fakePayload->kind == FakePayloadKind::file && round.fakePayload->value.empty())
        {
            diagnostics.push_back(errorAt("zapret.fake_payload.missing",
                                          std::string{roundPath} + ".fakePayload"));
            return;
        }
        arguments.option(fakeOptionFor(round), payloadValue(*round.fakePayload));
    }
}

void appendExtraArguments(ArgumentList& arguments, const Profile& profile)
{
    for (const std::string& argument : profile.extraArguments)
    {
        if (argument.empty())
        {
            continue;
        }
        arguments.flag(argument);
    }
}

void appendRoundFilter(ArgumentList& arguments, const DesyncRound& round, bool includeL7, bool isTpws)
{
    if (!round.filter.tcpPorts.empty())
    {
        arguments.option(isTpws ? "--port" : "--filter-tcp", serializePorts(round.filter.tcpPorts));
    }
    if (!round.filter.udpPorts.empty() && !isTpws)
    {
        arguments.option("--filter-udp", serializePorts(round.filter.udpPorts));
    }
    if (includeL7 && !round.filter.l7.empty())
    {
        arguments.option("--filter-l7", serializeL7(round.filter.l7));
    }
}

std::string toolExecutableOrFallback(const ToolPaths& paths, ToolKind tool)
{
    const auto configured = paths.pathFor(tool);
    if (!configured.empty())
    {
        return configured.string();
    }
    return std::string{platformToolExecutableName(tool)};
}

LaunchSpec buildWinwsLaunch(const Profile& profile,
                            const ToolPaths& paths,
                            std::string_view workingDirectory,
                            std::vector<Diagnostic>& diagnostics)
{
    LaunchSpec launch;
    launch.executable = toolExecutableOrFallback(paths, ToolKind::winws);
    launch.workingDirectory = std::string{workingDirectory};
    launch.label = profile.id;

    std::vector<std::uint16_t> tcpPorts = profile.winDivertTcpPorts;
    std::vector<std::uint16_t> udpPorts = profile.winDivertUdpPorts;
    if (tcpPorts.empty() && udpPorts.empty())
    {
        for (const DesyncRound& round : profile.rounds)
        {
            appendUniquePorts(tcpPorts, round.filter.tcpPorts);
            appendUniquePorts(udpPorts, round.filter.udpPorts);
        }
    }

    if (tcpPorts.empty() && udpPorts.empty())
    {
        diagnostics.push_back(errorAt("zapret.windivert.no_ports", "profiles." + profile.id));
    }

    ArgumentList arguments{launch.arguments};
    if (!tcpPorts.empty())
    {
        arguments.option("--wf-tcp", serializePorts(tcpPorts));
    }
    if (!udpPorts.empty())
    {
        arguments.option("--wf-udp", serializePorts(udpPorts));
    }

    bool first = true;
    for (std::size_t index = 0; index < profile.rounds.size(); ++index)
    {
        const DesyncRound& round = profile.rounds[index];
        const std::string roundPath = "profiles." + profile.id + ".rounds[" + std::to_string(index) + "]";

        if (!first)
        {
            arguments.flag("--new");
        }
        first = false;

        appendRoundFilter(arguments, round, true, false);
        appendListArguments(arguments, profile, round, paths, workingDirectory, roundPath, false, diagnostics);
        appendDesyncArguments(arguments, round, roundPath, diagnostics);
    }

    appendExtraArguments(arguments, profile);
    return launch;
}

std::vector<LaunchSpec> buildMultiProcessLaunches(const Profile& profile,
                                                  const ToolPaths& paths,
                                                  ToolKind tool,
                                                  std::string_view workingDirectory,
                                                  std::vector<Diagnostic>& diagnostics)
{
    std::vector<LaunchSpec> launches;
    const bool isTpws = tool == ToolKind::tpws;

    for (std::size_t index = 0; index < profile.rounds.size(); ++index)
    {
        const DesyncRound& round = profile.rounds[index];
        const std::string roundPath = "profiles." + profile.id + ".rounds[" + std::to_string(index) + "]";

        LaunchSpec launch;
        launch.executable = toolExecutableOrFallback(paths, tool);
        launch.workingDirectory = std::string{workingDirectory};
        launch.label = profile.id + " #" + std::to_string(index + 1);

        ArgumentList arguments{launch.arguments};

        if (!isTpws)
        {
            const int queueNumber = static_cast<int>(profile.nfqueue) + static_cast<int>(index);
            arguments.option("--qnum", queueNumber);
        }

        appendRoundFilter(arguments, round, !isTpws, isTpws);

        if (isTpws)
        {
            if (round.modes.empty())
            {
                diagnostics.push_back(errorAt("zapret.tpws.no_mode", roundPath + ".modes"));
            }
            // tpws always splits at split-pos; --disorder reorders the two segments.
            arguments.option("--split-pos", round.splitPos.value_or(1));
            if (containsMode(round.modes, DesyncMode::disorder) || containsMode(round.modes, DesyncMode::disorder2))
            {
                arguments.flag("--disorder");
            }
            if (round.splitSeqOverlap.has_value())
            {
                diagnostics.push_back(warningAt("zapret.tpws.seqovl_ignored", roundPath + ".splitSeqOverlap"));
            }
        }
        else
        {
            appendDesyncArguments(arguments, round, roundPath, diagnostics);
        }

        appendListArguments(arguments, profile, round, paths, workingDirectory, roundPath, isTpws, diagnostics);
        appendExtraArguments(arguments, profile);
        launches.push_back(std::move(launch));
    }

    return launches;
}

} // namespace

std::string LaunchSpec::renderCommandLine() const
{
    std::string rendered = strings::quoteForDisplay(executable);
    for (const std::string& argument : arguments)
    {
        rendered += " ";
        rendered += strings::quoteForDisplay(argument);
    }
    return rendered;
}

BuildResult buildLaunches(const Profile& profile,
                          ToolKind tool,
                          const ToolPaths& paths,
                          std::string_view workingDirectory)
{
    BuildResult result;

    if (!profile.enabled)
    {
        result.diagnostics.push_back(
            warningAt("zapret.profile.disabled", "profiles." + profile.id, {profile.id}));
    }

    if (profile.rounds.empty())
    {
        result.diagnostics.push_back(
            errorAt("zapret.profile.no_rounds", "profiles." + profile.id + ".rounds"));
        return result;
    }

    for (std::size_t index = 0; index < profile.rounds.size(); ++index)
    {
        const std::string roundPath = "profiles." + profile.id + ".rounds[" + std::to_string(index) + "]";
        const auto roundDiagnostics = validateRound(profile.rounds[index], roundPath, tool);
        result.diagnostics.insert(result.diagnostics.end(), roundDiagnostics.begin(), roundDiagnostics.end());
    }
    if (result.hasErrors())
    {
        return result;
    }

#ifdef _WIN32
    constexpr bool IS_WINDOWS = true;
#else
    constexpr bool IS_WINDOWS = false;
#endif

    if (tool == ToolKind::winws && !IS_WINDOWS)
    {
        result.diagnostics.push_back(warningAt("zapret.tool.windows_only", "profiles." + profile.id + ".tool"));
    }
    if ((tool == ToolKind::nfqws || tool == ToolKind::tpws) && IS_WINDOWS)
    {
        result.diagnostics.push_back(warningAt("zapret.tool.unix_only", "profiles." + profile.id + ".tool"));
    }

    if (!toolExecutableOrFallback(paths, tool).empty() && !paths.has(tool))
    {
        result.diagnostics.push_back(warningAt("zapret.tool.not_found",
                                               "profiles." + profile.id + ".tool",
                                               {std::string{toCliToken(tool)}}));
    }

    if (!workingDirectory.empty())
    {
        std::error_code code;
        if (!std::filesystem::is_directory(std::filesystem::path{workingDirectory}, code))
        {
            result.diagnostics.push_back(warningAt("zapret.working_directory.missing",
                                                   "zapret.workingDirectory",
                                                   {std::string{workingDirectory}}));
        }
    }

    if (tool == ToolKind::winws)
    {
        result.launches.push_back(buildWinwsLaunch(profile, paths, workingDirectory, result.diagnostics));
    }
    else
    {
        result.launches = buildMultiProcessLaunches(profile, paths, tool, workingDirectory, result.diagnostics);
    }

    return result;
}

BuildResult buildLaunchesForPlatform(const Profile& profile, const ToolPaths& paths, std::string_view workingDirectory)
{
    ToolKind tool = profile.tool;
    if (tool == ToolKind::autoDetect)
    {
        const auto resolved = resolveTool(ToolKind::autoDetect, paths);
        tool = resolved.hasValue() ? resolved.value() : platformDefaultTool();
    }
    return buildLaunches(profile, tool, paths, workingDirectory);
}

}
