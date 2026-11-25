#include <catch2/catch_test_macros.hpp>

#include <deimos/support/strings.hpp>
#include <deimos/transport/daemon_client.hpp>
#include <deimos/transport/protocol.hpp>

using namespace deimos;

TEST_CASE("formatEndpointUrl_FormatsIpLiteralsAndNonDefaultPort", "[protocol]")
{
    REQUIRE(strings::formatEndpointUrl("127.0.0.1", 8787) == "127.0.0.1:8787");
    REQUIRE(strings::formatEndpointUrl("::1", 18793) == "[::1]:18793");
    REQUIRE(strings::formatEndpointUrl("[2001:db8::1]", 18793) == "[2001:db8::1]:18793");
}

TEST_CASE("endpointDescribe_FormatsIpv6AndPreservesIpv4", "[protocol]")
{
    Endpoint ipv4;
    ipv4.host = "127.0.0.1";
    ipv4.port = 18793;
    REQUIRE(ipv4.describe() == "http://127.0.0.1:18793");

    Endpoint ipv6;
    ipv6.scheme = "https";
    ipv6.host = "2001:db8::1";
    ipv6.port = 18793;
    ipv6.basePath = "/deimos";
    REQUIRE(ipv6.describe() == "https://[2001:db8::1]:18793/deimos");
}

TEST_CASE("engineSnapshot_RoundTrips", "[protocol]")
{
    EngineSnapshot original;
    original.state = EngineState::running;
    original.activeProfileId = "tls-split";
    original.uptimeSeconds = 42;
    original.restartCount = 3;
    original.lastError = "none";
    original.launches.push_back(LaunchInfo{"tls-split", "winws.exe --wf-tcp=443", 4242});
    original.diagnostics.push_back(warning("round.disorder.no_spoofing", "needs fooling"));

    const std::string text = serializeEngineSnapshot(original, true);

    EngineSnapshot parsed;
    const Status status = parseEngineSnapshot(text, parsed);

    REQUIRE(status.isOk());
    REQUIRE(parsed.state == EngineState::running);
    REQUIRE(parsed.activeProfileId == "tls-split");
    REQUIRE(parsed.uptimeSeconds == 42);
    REQUIRE(parsed.restartCount == 3);
    REQUIRE(parsed.launches.size() == 1);
    REQUIRE(parsed.launches[0].pid == 4242);
    REQUIRE(parsed.diagnostics.size() == 1);
    REQUIRE(parsed.diagnostics[0].severity == Severity::warning);
}

TEST_CASE("daemonInfo_RoundTrips", "[protocol]")
{
    DaemonInfo original;
    original.version = "1.2.3";
    original.platform = "windows";
    original.architecture = "x86_64";
    original.hostname = "router";
    original.listenAddress = "127.0.0.1";
    original.port = 8787;
    original.uptimeSeconds = 90;
    original.authenticationRequired = true;
    original.zapretAvailable = true;
    original.zapretBaseDirectory = "C:/zapret";
    original.zapretTools = {"winws"};
    original.engine.state = EngineState::stopped;

    const std::string text = serializeDaemonInfo(original, true);

    DaemonInfo parsed;
    REQUIRE(parseDaemonInfo(text, parsed).isOk());
    REQUIRE(parsed.version == "1.2.3");
    REQUIRE(parsed.port == 8787);
    REQUIRE(parsed.zapretAvailable);
    REQUIRE(parsed.zapretTools.size() == 1);
    REQUIRE(parsed.engine.state == EngineState::stopped);
}

TEST_CASE("errorPayload_RoundTrips", "[protocol]")
{
    const Error original{ErrorCode::unauthorized, "missing token", "header absent"};
    const std::string text = serializeErrorPayload(original);

    Error parsed;
    REQUIRE(parseErrorPayload(text, parsed).isOk());
    REQUIRE(parsed.code() == ErrorCode::unauthorized);
    REQUIRE(parsed.message() == "missing token");
    REQUIRE(parsed.detail() == "header absent");
}

TEST_CASE("parseErrorPayload_RejectsUnrelatedJson", "[protocol]")
{
    Error parsed;
    REQUIRE_FALSE(parseErrorPayload(R"({"ok":true})", parsed).isOk());
    REQUIRE_FALSE(parseErrorPayload("not json", parsed).isOk());
}

TEST_CASE("logEntries_RoundTrip", "[protocol]")
{
    std::vector<LogEntry> entries;
    LogEntry first;
    first.sequence = 7;
    first.timestamp = std::chrono::system_clock::now();
    first.level = LogLevel::warning;
    first.source = "engine";
    first.message = "profile started";
    entries.push_back(first);

    const std::string text = serializeLogEntries(entries, 7);

    std::vector<LogEntry> parsedEntries;
    std::uint64_t lastSequence = 0;
    REQUIRE(parseLogEntries(text, parsedEntries, lastSequence).isOk());
    REQUIRE(lastSequence == 7);
    REQUIRE(parsedEntries.size() == 1);
    REQUIRE(parsedEntries[0].level == LogLevel::warning);
    REQUIRE(parsedEntries[0].source == "engine");
    REQUIRE(parsedEntries[0].message == "profile started");
    REQUIRE(parsedEntries[0].sequence == 7);
}

TEST_CASE("commandPreview_RoundTrips", "[protocol]")
{
    CommandPreview preview;
    preview.commands = {"winws.exe --wf-tcp=443", "nfqws --qnum=200"};
    preview.diagnostics = {info("preview.info", "preview only")};

    const std::string text = serializeCommandPreview(preview, true);

    CommandPreview parsed;
    REQUIRE(parseCommandPreview(text, parsed).isOk());
    REQUIRE(parsed.commands.size() == 2);
    REQUIRE(parsed.diagnostics.size() == 1);
}

TEST_CASE("parseEngineState_AcceptsKnownValues", "[protocol]")
{
    EngineState state = EngineState::faulted;
    REQUIRE(parseEngineState("running", state));
    REQUIRE(state == EngineState::running);
    REQUIRE_FALSE(parseEngineState("teleporting", state));
}
