#include <algorithm>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <deimos/zapret/command_builder.hpp>

using namespace deimos;

namespace
{

bool hasArgument(const std::vector<std::string>& arguments, const std::string& expected)
{
    return std::find(arguments.begin(), arguments.end(), expected) != arguments.end();
}

DesyncRound tlsSplitRound()
{
    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.filter.l7 = {L7Protocol::tls};
    round.modes = {DesyncMode::split2};
    round.splitPos = 1;
    round.autoTtl = 2;
    return round;
}

Profile singleRoundProfile()
{
    Profile profile;
    profile.id = "test";
    profile.name = "Test";
    profile.rounds.push_back(tlsSplitRound());
    return profile;
}

} // namespace

TEST_CASE("buildLaunches_WinwsProducesSingleProcess", "[zapret]")
{
    ToolPaths paths;
    paths.winws = "C:/zapret/binaries/windows-x86_64/winws.exe";

    const BuildResult result = buildLaunches(singleRoundProfile(), ToolKind::winws, paths, "");

    REQUIRE(result.launches.size() == 1);
    const LaunchSpec& launch = result.launches[0];
    REQUIRE(launch.executable == paths.winws.string());
    REQUIRE(hasArgument(launch.arguments, "--wf-tcp=443"));
    REQUIRE(hasArgument(launch.arguments, "--filter-tcp=443"));
    REQUIRE(hasArgument(launch.arguments, "--filter-l7=tls"));
    REQUIRE(hasArgument(launch.arguments, "--dpi-desync=split2"));
    REQUIRE(hasArgument(launch.arguments, "--dpi-desync-split-pos=1"));
    REQUIRE(hasArgument(launch.arguments, "--dpi-desync-autottl=2"));
    REQUIRE_FALSE(hasArgument(launch.arguments, "--new"));
}

TEST_CASE("buildLaunches_WinwsSeparatesRoundsWithNew", "[zapret]")
{
    Profile profile = singleRoundProfile();
    DesyncRound udpRound;
    udpRound.filter.udpPorts = {443};
    udpRound.filter.l7 = {L7Protocol::quic};
    udpRound.modes = {DesyncMode::fake};
    udpRound.repeats = 6;
    profile.rounds.push_back(udpRound);

    ToolPaths paths;
    paths.winws = "winws.exe";

    const BuildResult result = buildLaunches(profile, ToolKind::winws, paths, "");

    REQUIRE(result.launches.size() == 1);
    REQUIRE(hasArgument(result.launches[0].arguments, "--new"));
    REQUIRE(hasArgument(result.launches[0].arguments, "--wf-udp=443"));
    REQUIRE(hasArgument(result.launches[0].arguments, "--dpi-desync-repeats=6"));
}

TEST_CASE("buildLaunches_NfqwsSpawnsOneProcessPerRound", "[zapret]")
{
    Profile profile = singleRoundProfile();
    profile.nfqueue = 200;
    DesyncRound second = tlsSplitRound();
    second.filter.tcpPorts = {8443};
    profile.rounds.push_back(second);

    ToolPaths paths;
    paths.nfqws = "/opt/zapret/nfqws";

    const BuildResult result = buildLaunches(profile, ToolKind::nfqws, paths, "");

    REQUIRE(result.launches.size() == 2);
    REQUIRE(hasArgument(result.launches[0].arguments, "--qnum=200"));
    REQUIRE(hasArgument(result.launches[1].arguments, "--qnum=201"));
    REQUIRE(hasArgument(result.launches[0].arguments, "--filter-tcp=443"));
    REQUIRE(hasArgument(result.launches[1].arguments, "--filter-tcp=8443"));
    REQUIRE_FALSE(hasArgument(result.launches[0].arguments, "--new"));
}

TEST_CASE("buildLaunches_TpwsMapsSplitAndDisorder", "[zapret]")
{
    Profile profile;
    profile.id = "proxy";
    profile.tool = ToolKind::tpws;

    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.modes = {DesyncMode::disorder};
    round.splitPos = 2;
    profile.rounds.push_back(round);

    ToolPaths paths;
    paths.tpws = "/opt/zapret/tpws";

    const BuildResult result = buildLaunches(profile, ToolKind::tpws, paths, "");

    INFO(formatDiagnostics(result.diagnostics));
    REQUIRE(result.launches.size() == 1);
    REQUIRE(hasArgument(result.launches[0].arguments, "--port=443"));
    REQUIRE(hasArgument(result.launches[0].arguments, "--split-pos=2"));
    REQUIRE(hasArgument(result.launches[0].arguments, "--disorder"));
}

TEST_CASE("buildLaunches_TpwsRejectsFakeModes", "[zapret]")
{
    Profile profile;
    profile.id = "proxy";
    profile.tool = ToolKind::tpws;

    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.modes = {DesyncMode::fake};
    profile.rounds.push_back(round);

    ToolPaths paths;
    paths.tpws = "tpws";

    const BuildResult result = buildLaunches(profile, ToolKind::tpws, paths, "");

    REQUIRE(result.hasErrors());
}

TEST_CASE("buildLaunches_InlineHexPayloadUsesAtZeroX", "[zapret]")
{
    Profile profile;
    profile.id = "inline";
    DesyncRound round;
    round.filter.udpPorts = {443};
    round.filter.l7 = {L7Protocol::quic};
    round.modes = {DesyncMode::fake};
    round.fakePayload = FakePayload{FakePayloadKind::hex, "01020304"};
    profile.rounds.push_back(round);

    ToolPaths paths;
    paths.winws = "winws.exe";

    const BuildResult result = buildLaunches(profile, ToolKind::winws, paths, "");

    REQUIRE(hasArgument(result.launches[0].arguments, "--dpi-desync-fake-quic=@0x01020304"));
}

TEST_CASE("buildLaunches_WarnsWhenToolIsMissing", "[zapret]")
{
    ToolPaths empty;

    const BuildResult result = buildLaunches(singleRoundProfile(), ToolKind::winws, empty, "");

    REQUIRE_FALSE(result.hasErrors());
    bool sawWarning = false;
    for (const Diagnostic& diagnostic : result.diagnostics)
    {
        if (diagnostic.code == "zapret.tool.not_found")
        {
            sawWarning = true;
        }
    }
    REQUIRE(sawWarning);
}

TEST_CASE("buildLaunches_ProfileWithoutRoundsFails", "[zapret]")
{
    Profile profile;
    profile.id = "empty";

    ToolPaths paths;
    paths.winws = "winws.exe";

    const BuildResult result = buildLaunches(profile, ToolKind::winws, paths, "");

    REQUIRE(result.hasErrors());
    REQUIRE(result.launches.empty());
}

TEST_CASE("buildLaunches_AppendsExtraArguments", "[zapret]")
{
    Profile profile = singleRoundProfile();
    profile.extraArguments = {"--debug=2"};

    ToolPaths paths;
    paths.winws = "winws.exe";

    const BuildResult result = buildLaunches(profile, ToolKind::winws, paths, "");

    REQUIRE(hasArgument(result.launches[0].arguments, "--debug=2"));
}

TEST_CASE("launchSpec_RendersQuotedCommandLine", "[zapret]")
{
    LaunchSpec launch;
    launch.executable = "C:/Program Files/zapret/winws.exe";
    launch.arguments = {"--wf-tcp=443", "--hostlist=C:/lists/my list.txt"};

    const std::string rendered = launch.renderCommandLine();
    REQUIRE(rendered.find("\"C:/Program Files/zapret/winws.exe\"") != std::string::npos);
    REQUIRE(rendered.find("--wf-tcp=443") != std::string::npos);
    REQUIRE(rendered.find("\"--hostlist=C:/lists/my list.txt\"") != std::string::npos);
}
