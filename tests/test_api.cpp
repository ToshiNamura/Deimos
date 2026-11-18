#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "api_server.hpp"
#include "config_store.hpp"
#include "engine.hpp"

#include <deimos/transport/daemon_client.hpp>

using namespace deimos;

namespace
{

std::string uniqueSuffix()
{
    static int counter = 0;
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::to_string(ticks) + "-" + std::to_string(++counter);
}

struct TestDaemon
{
    std::filesystem::path directory;
    std::unique_ptr<ConfigStore> store;
    std::unique_ptr<RuntimeEngine> engine;
    std::unique_ptr<ApiServer> server;
    std::uint16_t port = 0;
    std::string token{"test-token"};

    TestDaemon()
    {
        directory = std::filesystem::temp_directory_path() / ("deimos-api-" + uniqueSuffix());
        std::filesystem::create_directories(directory);

        store = std::make_unique<ConfigStore>((directory / "deimos.json").string());
        REQUIRE(store->load().isOk());

        engine = std::make_unique<RuntimeEngine>(EngineContext{});

        ApiServerDependencies dependencies;
        dependencies.configStore = store.get();
        dependencies.engine = engine.get();
        dependencies.logs = std::make_shared<LogRingBuffer>(100);
        dependencies.version = "test";
        dependencies.startedAt = std::chrono::system_clock::now();

        server = std::make_unique<ApiServer>(dependencies);
        server->setToken(token);
        REQUIRE(server->startEphemeral().isOk());
        port = server->boundPort();
    }

    ~TestDaemon()
    {
        if (server)
        {
            server->stop();
        }
        std::error_code code;
        std::filesystem::remove_all(directory, code);
    }

    ClientOptions clientOptions(std::string tokenValue) const
    {
        ClientOptions options;
        options.endpoint.scheme = "http";
        options.endpoint.host = "127.0.0.1";
        options.endpoint.port = port;
        options.token = std::move(tokenValue);
        options.timeoutSeconds = 10;
        return options;
    }
};

} // namespace

TEST_CASE("api_HealthIsAvailableWithoutToken", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions({})};

    const auto health = client.health();
    if (!health.hasValue())
    {
        INFO(health.error().describe());
    }
    REQUIRE(health.hasValue());
    REQUIRE(health.value().status == "ok");
    REQUIRE(health.value().version == "test");
}

TEST_CASE("api_StatusRejectsMissingToken", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions({})};

    const auto status = client.status();
    REQUIRE_FALSE(status.hasValue());
    REQUIRE(status.error().code() == ErrorCode::unauthorized);
}

TEST_CASE("api_StatusReturnsDaemonInfo", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions(daemon.token)};

    const auto status = client.status();
    if (!status.hasValue())
    {
        INFO(status.error().describe());
    }
    REQUIRE(status.hasValue());
    REQUIRE(status.value().version == "test");
    REQUIRE(status.value().port == daemon.store->config().daemon.port);
    REQUIRE(status.value().engine.state == EngineState::stopped);
}

TEST_CASE("api_ProfileCrudRoundTrip", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions(daemon.token)};

    Profile profile;
    profile.id = "api-profile";
    profile.name = "API profile";
    profile.tool = ToolKind::nfqws;

    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.modes = {DesyncMode::split};
    round.splitPos = 2;
    profile.rounds.push_back(round);

    const auto saved = client.saveProfile(profile);
    if (!saved.hasValue())
    {
        INFO(saved.error().describe());
    }
    REQUIRE(saved.hasValue());
    REQUIRE(saved.value().id == "api-profile");

    const auto listed = client.listProfiles();
    REQUIRE(listed.hasValue());
    bool found = false;
    for (const ProfileSummary& summary : listed.value())
    {
        if (summary.id == "api-profile")
        {
            found = true;
            REQUIRE(summary.roundCount == 1);
        }
    }
    REQUIRE(found);

    const auto fetched = client.getProfile("api-profile");
    REQUIRE(fetched.hasValue());
    REQUIRE(fetched.value().name == "API profile");
    REQUIRE(fetched.value().rounds.size() == 1);
    REQUIRE(fetched.value().rounds[0].splitPos.value_or(0) == 2);

    REQUIRE(client.deleteProfile("api-profile").isOk());
    REQUIRE_FALSE(client.getProfile("api-profile").hasValue());
}

TEST_CASE("api_PreviewReturnsCommandLines", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions(daemon.token)};

    const auto preview = client.previewProfile("tls-split");
    if (!preview.hasValue())
    {
        INFO(preview.error().describe());
    }
    REQUIRE(preview.hasValue());
    REQUIRE(preview.value().commands.size() == 1);
    REQUIRE(preview.value().commands[0].find("--filter-tcp=443") != std::string::npos);
}

TEST_CASE("api_UnknownEndpointReturnsNotFound", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions(daemon.token)};

    const auto response = client.call("GET", "/api/v1/does-not-exist");
    REQUIRE_FALSE(response.hasValue());
    REQUIRE(response.error().code() == ErrorCode::notFound);
}

TEST_CASE("api_EngineStartWithoutToolFailsButDaemonSurvives", "[api]")
{
    TestDaemon daemon;

    EngineContext context;
    context.paths.nfqws = "/nonexistent/deimos-nfqws";
    context.settings.maxRestarts = 0;
    REQUIRE(daemon.engine->updateContext(context).isOk());

    const DaemonClient client{daemon.clientOptions(daemon.token)};

    Profile profile;
    profile.id = "broken-tool";
    profile.name = "Broken";
    profile.tool = ToolKind::nfqws;
    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.modes = {DesyncMode::split};
    profile.rounds.push_back(round);
    REQUIRE(client.saveProfile(profile).hasValue());

    const auto started = client.startEngine("broken-tool");
    if (started.hasValue())
    {
        INFO(std::string{"unexpected success"});
    }
    else
    {
        INFO(started.error().describe());
    }
    REQUIRE_FALSE(started.hasValue());

    // The daemon must keep serving after a failed spawn instead of crashing.
    REQUIRE(client.health().hasValue());
    const auto status = client.status();
    REQUIRE(status.hasValue());
    REQUIRE(status.value().engine.state != EngineState::running);
}

TEST_CASE("api_ConfigRoundTrip", "[api]")
{
    TestDaemon daemon;
    const DaemonClient client{daemon.clientOptions(daemon.token)};

    const auto config = client.getConfig();
    REQUIRE(config.hasValue());
    REQUIRE_FALSE(config.value().profiles.empty());

    DeimosConfig updated = config.value();
    updated.daemon.port = 9999;
    REQUIRE(client.putConfig(updated).isOk());

    const auto reloaded = client.getConfig();
    REQUIRE(reloaded.hasValue());
    REQUIRE(reloaded.value().daemon.port == 9999);
}
