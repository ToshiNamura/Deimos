#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "config_store.hpp"

#include <deimos/model/config.hpp>
#include <deimos/model/profile.hpp>

using namespace deimos;

namespace
{

DeimosConfig parseOrFail(const std::string& text, std::vector<Diagnostic>& diagnostics)
{
    DeimosConfig config;
    const Status status = deserializeConfig(text, config, diagnostics);
    INFO(formatDiagnostics(diagnostics));
    REQUIRE(status.isOk());
    return config;
}

} // namespace

TEST_CASE("defaultConfig_IsValid", "[config]")
{
    const DeimosConfig config = defaultConfig();
    const std::vector<Diagnostic> diagnostics = validateConfig(config);

    INFO(formatDiagnostics(diagnostics));
    REQUIRE_FALSE(hasErrors(diagnostics));
    REQUIRE(config.profiles.size() == 2);
    REQUIRE(config.findProfile("tls-split") != nullptr);
    REQUIRE(config.daemon.activeProfileId == "tls-split");
}

TEST_CASE("serializeConfig_RoundTrips", "[config]")
{
    const DeimosConfig original = defaultConfig();
    const std::string text = serializeConfig(original, true);

    std::vector<Diagnostic> diagnostics;
    const DeimosConfig restored = parseOrFail(text, diagnostics);

    REQUIRE(restored.schemaVersion == original.schemaVersion);
    REQUIRE(restored.daemon.port == original.daemon.port);
    REQUIRE(restored.daemon.activeProfileId == original.daemon.activeProfileId);
    REQUIRE(restored.profiles.size() == original.profiles.size());

    REQUIRE(restored.profiles[0].id == original.profiles[0].id);
    REQUIRE(restored.profiles[0].rounds.size() == original.profiles[0].rounds.size());
    REQUIRE(restored.profiles[0].rounds[0].modes == original.profiles[0].rounds[0].modes);
    REQUIRE(restored.profiles[0].rounds[0].splitPos == original.profiles[0].rounds[0].splitPos);
    REQUIRE(restored.profiles[0].rounds[0].filter.tcpPorts == original.profiles[0].rounds[0].filter.tcpPorts);
    REQUIRE(restored.profiles[1].rounds[0].fakePayload.has_value());
    REQUIRE(restored.profiles[1].rounds[0].fakePayload->value ==
            original.profiles[1].rounds[0].fakePayload->value);
}

TEST_CASE("deserializeConfig_RejectsPortOutOfRange", "[config]")
{
    const std::string text = R"({"schemaVersion":1,"daemon":{"port":70000}})";

    DeimosConfig config;
    std::vector<Diagnostic> diagnostics;
    const Status status = deserializeConfig(text, config, diagnostics);

    REQUIRE_FALSE(status.isOk());
    REQUIRE(hasErrors(diagnostics));
    REQUIRE(status.error().code() == ErrorCode::configuration);
}

TEST_CASE("deserializeConfig_RejectsNewerSchema", "[config]")
{
    const std::string text = R"({"schemaVersion":99})";

    DeimosConfig config;
    std::vector<Diagnostic> diagnostics;
    const Status status = deserializeConfig(text, config, diagnostics);

    REQUIRE_FALSE(status.isOk());
    REQUIRE(status.error().code() == ErrorCode::notSupported);
}

TEST_CASE("deserializeConfig_RejectsUnknownEnumValue", "[config]")
{
    const std::string text = R"({
        "schemaVersion": 1,
        "profiles": [{
            "id": "broken",
            "name": "Broken",
            "rounds": [{"filter": {"tcp": [443]}, "modes": ["teleport"]}]
        }]
    })";

    DeimosConfig config;
    std::vector<Diagnostic> diagnostics;
    const Status status = deserializeConfig(text, config, diagnostics);

    REQUIRE_FALSE(status.isOk());
    bool sawUnknownValue = false;
    for (const Diagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.code == "config.unknown_value")
        {
            sawUnknownValue = true;
        }
    }
    REQUIRE(sawUnknownValue);
}

TEST_CASE("validateConfig_DetectsDuplicateProfileIds", "[config]")
{
    DeimosConfig config = defaultConfig();
    config.profiles.push_back(config.profiles.front());

    const std::vector<Diagnostic> diagnostics = validateConfig(config);
    REQUIRE(hasErrors(diagnostics));
}

TEST_CASE("validateConfig_RemoteListenRequiresOptIn", "[config]")
{
    DeimosConfig config = defaultConfig();
    config.daemon.listenAddress = "0.0.0.0";

    const std::vector<Diagnostic> diagnostics = validateConfig(config);
    REQUIRE(hasErrors(diagnostics));

    config.daemon.allowRemoteClients = true;
    const std::vector<Diagnostic> afterOptIn = validateConfig(config);
    REQUIRE_FALSE(hasErrors(afterOptIn));
}

TEST_CASE("validateConfig_MissingActiveProfileIsAnError", "[config]")
{
    DeimosConfig config = defaultConfig();
    config.daemon.activeProfileId = "does-not-exist";

    REQUIRE(hasErrors(validateConfig(config)));
}

TEST_CASE("validateConfig_TlsNeedsBothFiles", "[config]")
{
    DeimosConfig config = defaultConfig();
    config.daemon.enableTls = true;

    REQUIRE(hasErrors(validateConfig(config)));
}

TEST_CASE("profile_UpsertAndRemove", "[config]")
{
    DeimosConfig config = defaultConfig();

    Profile extra;
    extra.id = "custom";
    extra.name = "Custom";
    DesyncRound round;
    round.filter.tcpPorts = {8443};
    round.modes = {DesyncMode::split};
    extra.rounds.push_back(round);

    REQUIRE_FALSE(config.upsertProfile(extra));
    REQUIRE(config.profiles.size() == 3);
    REQUIRE(config.findProfile("custom") != nullptr);

    extra.name = "Renamed";
    REQUIRE(config.upsertProfile(extra));
    REQUIRE(config.findProfile("custom")->name == "Renamed");

    REQUIRE(config.removeProfile("custom"));
    REQUIRE(config.findProfile("custom") == nullptr);
    REQUIRE_FALSE(config.removeProfile("custom"));
}

TEST_CASE("slugify_NormalisesText", "[config]")
{
    REQUIRE(slugify("TLS Split (443)") == "tls-split-443");
    REQUIRE(slugify("  ---  ") == "");
    REQUIRE(slugify("YouTube QUIC") == "youtube-quic");
}

TEST_CASE("configStore_LoadMissingPath_DoesNotWriteFile", "[config]")
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("deimos-config-" + std::to_string(ticks));
    const std::filesystem::path path = directory / "deimos.json";
    std::error_code errorCode;
    std::filesystem::remove_all(directory, errorCode);

    ConfigStore store(path.string());
    REQUIRE(store.load().isOk());
    REQUIRE(store.config().daemon.activeProfileId == "tls-split");

    // A read-only load (the --validate-config path) must not persist anything.
    REQUIRE_FALSE(std::filesystem::exists(path));

    // Persisting defaults is an explicit operation used by the daemon startup path.
    REQUIRE(store.save().isOk());
    REQUIRE(std::filesystem::exists(path));

    std::filesystem::remove_all(directory, errorCode);
}
