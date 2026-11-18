#include "api_server.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include <nlohmann/json.hpp>

#include <deimos/model/profile.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>
#include <deimos/transport/protocol.hpp>
#include <deimos/zapret/command_builder.hpp>
#include <deimos/zapret/toolchain.hpp>

namespace deimos
{
namespace
{

HttpResponse jsonResponse(int status, std::string body)
{
    HttpResponse response;
    response.status = status;
    response.body = std::move(body);
    return response;
}

HttpResponse errorResponse(ErrorCode code, std::string message, std::string detail = {})
{
    return jsonResponse(httpStatusFor(code), serializeErrorPayload(Error{code, std::move(message), std::move(detail)}));
}

HttpResponse errorResponse(const Error& error)
{
    return jsonResponse(httpStatusFor(error.code()), serializeErrorPayload(error));
}

bool constantTimeEquals(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    unsigned char difference = 0;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        difference = static_cast<unsigned char>(difference |
                                                (static_cast<unsigned char>(left[index]) ^
                                                 static_cast<unsigned char>(right[index])));
    }
    return difference == 0;
}

std::string fieldOrEmpty(const std::string& body, const char* field)
{
    if (body.empty())
    {
        return {};
    }
    try
    {
        const auto root = nlohmann::json::parse(body);
        if (!root.is_object())
        {
            return {};
        }
        const auto iterator = root.find(field);
        if (iterator == root.end() || !iterator->is_string())
        {
            return {};
        }
        return iterator->get<std::string>();
    }
    catch (const std::exception&)
    {
        return {};
    }
}

std::string pathParam(const HttpRequest& request, const char* name)
{
    const auto iterator = request.pathParams.find(name);
    return iterator == request.pathParams.end() ? std::string{} : iterator->second;
}

std::string workingDirectoryFor(const DeimosConfig& config, const ToolPaths& paths)
{
    return config.zapret.workingDirectory.empty() ? paths.baseDirectory.string() : config.zapret.workingDirectory;
}

} // namespace

ApiServer::ApiServer(ApiServerDependencies dependencies)
    : dependencies_(std::move(dependencies))
{
}

ApiServer::~ApiServer()
{
    stop();
}

void ApiServer::setToken(std::string token)
{
    std::lock_guard<std::mutex> lock(tokenMutex_);
    token_ = std::move(token);
}

std::optional<HttpResponse> ApiServer::authenticate(const HttpRequest& request) const
{
    if (request.path == "/api/v1/health" || request.path == "/health")
    {
        return std::nullopt;
    }

    const DeimosConfig config = dependencies_.configStore->config();
    if (!config.daemon.requireAuthentication)
    {
        return std::nullopt;
    }

    std::string expected;
    {
        std::lock_guard<std::mutex> lock(tokenMutex_);
        expected = token_;
    }
    if (expected.empty())
    {
        return std::nullopt;
    }

    const std::string provided = request.header("authorization");
    if (!constantTimeEquals(provided, "Bearer " + expected))
    {
        HttpResponse response = errorResponse(ErrorCode::unauthorized, "missing or invalid access token");
        response.headers["WWW-Authenticate"] = "Bearer";
        return response;
    }
    return std::nullopt;
}

HttpResponse ApiServer::handleStatus() const
{
    const DeimosConfig config = dependencies_.configStore->config();

    DaemonInfo info;
    info.version = dependencies_.version;
    info.platform = std::string{platform::platformName()};
    info.architecture = std::string{platform::architectureName()};
    info.hostname = platform::hostname();
    info.listenAddress = config.daemon.listenAddress;
    info.port = config.daemon.port;
    info.uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() -
                                                                         dependencies_.startedAt)
                             .count();
    info.authenticationRequired = config.daemon.requireAuthentication;

    const ToolPaths paths = dependencies_.engine->toolPaths();
    info.zapretAvailable = paths.any();
    info.zapretBaseDirectory = paths.baseDirectory.string();
    if (paths.has(ToolKind::winws))
    {
        info.zapretTools.emplace_back("winws");
    }
    if (paths.has(ToolKind::nfqws))
    {
        info.zapretTools.emplace_back("nfqws");
    }
    if (paths.has(ToolKind::tpws))
    {
        info.zapretTools.emplace_back("tpws");
    }

    info.engine = dependencies_.engine->snapshot();
    return jsonResponse(200, serializeDaemonInfo(info));
}

HttpResponse ApiServer::handleStartEngine(const HttpRequest& request)
{
    const DeimosConfig config = dependencies_.configStore->config();
    std::string profileId = fieldOrEmpty(request.body, "profileId");
    if (profileId.empty())
    {
        profileId = config.daemon.activeProfileId;
    }
    if (profileId.empty())
    {
        return errorResponse(ErrorCode::invalidArgument, "no profile selected",
                             "pass profileId or activate a profile first");
    }

    const Profile* profile = config.findProfile(profileId);
    if (profile == nullptr)
    {
        return errorResponse(ErrorCode::notFound, "profile not found", profileId);
    }

    const auto result = dependencies_.engine->start(*profile);
    if (!result)
    {
        return errorResponse(result.error());
    }
    return jsonResponse(200, serializeEngineSnapshot(result.value()));
}

HttpResponse ApiServer::handleStopEngine()
{
    const Status status = dependencies_.engine->stop();
    if (!status.isOk())
    {
        return errorResponse(status.error());
    }
    return jsonResponse(200, serializeEngineSnapshot(dependencies_.engine->snapshot()));
}

HttpResponse ApiServer::handleRestartEngine(const HttpRequest& request)
{
    const DeimosConfig config = dependencies_.configStore->config();
    std::string profileId = fieldOrEmpty(request.body, "profileId");
    if (profileId.empty())
    {
        profileId = dependencies_.engine->snapshot().activeProfileId;
    }
    if (profileId.empty())
    {
        profileId = config.daemon.activeProfileId;
    }

    const Profile* profile = config.findProfile(profileId);
    if (profile == nullptr)
    {
        return errorResponse(ErrorCode::notFound, "profile not found", profileId);
    }

    const auto result = dependencies_.engine->restart(*profile);
    if (!result)
    {
        return errorResponse(result.error());
    }
    return jsonResponse(200, serializeEngineSnapshot(result.value()));
}

HttpResponse ApiServer::handleListProfiles() const
{
    const DeimosConfig config = dependencies_.configStore->config();
    std::vector<ProfileSummary> summaries;
    summaries.reserve(config.profiles.size());
    for (const Profile& profile : config.profiles)
    {
        summaries.push_back(summarize(profile));
    }

    nlohmann::json root;
    root["profiles"] = nlohmann::json::parse(serializeProfileSummaries(summaries, false));
    root["activeProfileId"] = config.daemon.activeProfileId;
    return jsonResponse(200, root.dump());
}

HttpResponse ApiServer::handleGetProfile(const HttpRequest& request) const
{
    const DeimosConfig config = dependencies_.configStore->config();
    const std::string id = pathParam(request, "id");
    const Profile* profile = config.findProfile(id);
    if (profile == nullptr)
    {
        return errorResponse(ErrorCode::notFound, "profile not found", id);
    }
    return jsonResponse(200, serializeProfile(*profile));
}

HttpResponse ApiServer::handleSaveProfile(const HttpRequest& request)
{
    const std::string id = pathParam(request, "id");
    if (id.empty())
    {
        return errorResponse(ErrorCode::invalidArgument, "profile id is required");
    }

    Profile profile;
    std::vector<Diagnostic> diagnostics;
    const Status parseStatus = parseProfile(request.body, profile, diagnostics);
    if (!parseStatus.isOk())
    {
        nlohmann::json root;
        root["error"] = nlohmann::json{
            {"code", "invalid_argument"},
            {"message", "profile is invalid"},
            {"detail", ""},
        };
        root["diagnostics"] = nlohmann::json::parse(serializeDiagnostics(diagnostics, false));
        return jsonResponse(400, root.dump());
    }

    profile.id = id;

    DeimosConfig config = dependencies_.configStore->config();
    const bool replaced = config.upsertProfile(profile);
    dependencies_.configStore->setConfig(config);
    if (const Status saveStatus = dependencies_.configStore->save(); !saveStatus.isOk())
    {
        return errorResponse(saveStatus.error());
    }

    log::info("api", std::string{replaced ? "updated" : "created"} + " profile '" + id + "'");
    return jsonResponse(replaced ? 200 : 201, serializeProfile(profile));
}

HttpResponse ApiServer::handleDeleteProfile(const HttpRequest& request)
{
    const std::string id = pathParam(request, "id");

    DeimosConfig config = dependencies_.configStore->config();
    if (!config.removeProfile(id))
    {
        return errorResponse(ErrorCode::notFound, "profile not found", id);
    }
    if (config.daemon.activeProfileId == id)
    {
        config.daemon.activeProfileId.clear();
    }
    dependencies_.configStore->setConfig(config);
    if (const Status saveStatus = dependencies_.configStore->save(); !saveStatus.isOk())
    {
        return errorResponse(saveStatus.error());
    }

    log::info("api", "deleted profile '" + id + "'");
    return jsonResponse(200, R"({"deleted":"yes"})");
}

HttpResponse ApiServer::handleActivateProfile(const HttpRequest& request)
{
    const std::string id = pathParam(request, "id");

    DeimosConfig config = dependencies_.configStore->config();
    const Profile* profile = config.findProfile(id);
    if (profile == nullptr)
    {
        return errorResponse(ErrorCode::notFound, "profile not found", id);
    }

    config.daemon.activeProfileId = id;
    dependencies_.configStore->setConfig(config);
    if (const Status saveStatus = dependencies_.configStore->save(); !saveStatus.isOk())
    {
        return errorResponse(saveStatus.error());
    }

    if (dependencies_.engine->isRunning())
    {
        const auto restartResult = dependencies_.engine->restart(*profile);
        if (!restartResult)
        {
            return errorResponse(restartResult.error());
        }
        return jsonResponse(200, serializeEngineSnapshot(restartResult.value()));
    }

    return jsonResponse(200, serializeEngineSnapshot(dependencies_.engine->snapshot()));
}

HttpResponse ApiServer::handlePreviewProfile(const HttpRequest& request) const
{
    const DeimosConfig config = dependencies_.configStore->config();
    const std::string id = pathParam(request, "id");
    const Profile* profile = config.findProfile(id);
    if (profile == nullptr)
    {
        return errorResponse(ErrorCode::notFound, "profile not found", id);
    }

    const ToolPaths paths = dependencies_.engine->toolPaths();
    const BuildResult build =
        buildLaunchesForPlatform(*profile, paths, workingDirectoryFor(config, paths));

    CommandPreview preview;
    preview.diagnostics = build.diagnostics;
    for (const LaunchSpec& launch : build.launches)
    {
        preview.commands.push_back(launch.renderCommandLine());
    }
    return jsonResponse(200, serializeCommandPreview(preview));
}

HttpResponse ApiServer::handleGetConfig() const
{
    return jsonResponse(200, serializeConfig(dependencies_.configStore->config(), true));
}

HttpResponse ApiServer::handlePutConfig(const HttpRequest& request)
{
    DeimosConfig config;
    std::vector<Diagnostic> diagnostics;
    const Status status = deserializeConfig(request.body, config, diagnostics);
    if (!status.isOk())
    {
        nlohmann::json root;
        root["error"] = nlohmann::json{
            {"code", "configuration_error"},
            {"message", status.error().message()},
            {"detail", status.error().detail()},
        };
        root["diagnostics"] = nlohmann::json::parse(serializeDiagnostics(diagnostics, false));
        return jsonResponse(400, root.dump());
    }

    dependencies_.configStore->setConfig(config);
    if (const Status saveStatus = dependencies_.configStore->save(); !saveStatus.isOk())
    {
        return errorResponse(saveStatus.error());
    }

    if (dependencies_.onConfigChanged)
    {
        if (const Status applied = dependencies_.onConfigChanged(config); !applied.isOk())
        {
            return errorResponse(applied.error());
        }
    }

    return jsonResponse(200, serializeConfig(dependencies_.configStore->config(), true));
}

HttpResponse ApiServer::handleLogs(const HttpRequest& request) const
{
    std::size_t limit = 200;
    if (const std::string limitText = request.queryValue("limit"); !limitText.empty())
    {
        std::int64_t parsed = 0;
        if (strings::parseInt(limitText, parsed) && parsed > 0)
        {
            limit = std::min<std::size_t>(static_cast<std::size_t>(parsed), 2000);
        }
    }

    LogLevel minimum = LogLevel::trace;
    if (const std::string levelText = request.queryValue("level"); !levelText.empty())
    {
        LogLevel parsed = LogLevel::trace;
        if (parseLogLevel(levelText, parsed))
        {
            minimum = parsed;
        }
    }

    std::uint64_t after = 0;
    if (const std::string afterText = request.queryValue("after"); !afterText.empty())
    {
        std::int64_t parsed = 0;
        if (strings::parseInt(afterText, parsed) && parsed > 0)
        {
            after = static_cast<std::uint64_t>(parsed);
        }
    }

    if (!dependencies_.logs)
    {
        return jsonResponse(200, serializeLogEntries({}, 0));
    }

    const std::vector<LogEntry> entries = after == 0 ? dependencies_.logs->snapshot(limit, minimum)
                                                     : dependencies_.logs->snapshotAfter(after, limit, minimum);
    return jsonResponse(200, serializeLogEntries(entries, dependencies_.logs->lastSequence()));
}

HttpResponse ApiServer::handleZapret() const
{
    const DeimosConfig config = dependencies_.configStore->config();
    const ToolchainReport report = detectZapret(config.zapret);

    ZapretInfo info;
    info.available = report.paths.any();
    info.baseDirectory = report.paths.baseDirectory.string();
    info.filesDirectory = report.paths.filesDirectory.string();
    info.listsDirectory = report.paths.listsDirectory.string();
    if (!report.paths.winws.empty())
    {
        info.tools.emplace_back("winws");
    }
    if (!report.paths.nfqws.empty())
    {
        info.tools.emplace_back("nfqws");
    }
    if (!report.paths.tpws.empty())
    {
        info.tools.emplace_back("tpws");
    }
    info.diagnostics = report.diagnostics;
    return jsonResponse(200, serializeZapretInfo(info, true));
}

void ApiServer::registerRoutes()
{
    const auto prefix = [](std::string_view suffix)
    {
        return std::string{API_PREFIX} + "/" + std::string{suffix};
    };

    server_.route("GET", prefix("health"), [this](const HttpRequest&)
    {
        HealthInfo info;
        info.version = dependencies_.version;
        info.status = "ok";
        info.uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now() - dependencies_.startedAt)
                                 .count();
        return jsonResponse(200, serializeHealth(info));
    });
    server_.route("GET", "/health", [this](const HttpRequest&)
    {
        HealthInfo info;
        info.version = dependencies_.version;
        info.status = "ok";
        info.uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now() - dependencies_.startedAt)
                                 .count();
        return jsonResponse(200, serializeHealth(info));
    });

    server_.route("GET", prefix("status"), [this](const HttpRequest&)
    {
        return handleStatus();
    });
    server_.route("POST", prefix("engine/start"), [this](const HttpRequest& request)
    {
        return handleStartEngine(request);
    });
    server_.route("POST", prefix("engine/stop"), [this](const HttpRequest&)
    {
        return handleStopEngine();
    });
    server_.route("POST", prefix("engine/restart"), [this](const HttpRequest& request)
    {
        return handleRestartEngine(request);
    });

    server_.route("GET", prefix("profiles"), [this](const HttpRequest&)
    {
        return handleListProfiles();
    });
    server_.routePattern("GET", prefix("profiles/:id"), [this](const HttpRequest& request)
    {
        return handleGetProfile(request);
    });
    server_.routePattern("PUT", prefix("profiles/:id"), [this](const HttpRequest& request)
    {
        return handleSaveProfile(request);
    });
    server_.routePattern("DELETE", prefix("profiles/:id"), [this](const HttpRequest& request)
    {
        return handleDeleteProfile(request);
    });
    server_.routePattern("POST", prefix("profiles/:id/activate"), [this](const HttpRequest& request)
    {
        return handleActivateProfile(request);
    });
    server_.routePattern("GET", prefix("profiles/:id/preview"), [this](const HttpRequest& request)
    {
        return handlePreviewProfile(request);
    });

    server_.route("GET", prefix("config"), [this](const HttpRequest&)
    {
        return handleGetConfig();
    });
    server_.route("PUT", prefix("config"), [this](const HttpRequest& request)
    {
        return handlePutConfig(request);
    });

    server_.route("GET", prefix("logs"), [this](const HttpRequest& request)
    {
        return handleLogs(request);
    });
    server_.route("GET", prefix("zapret"), [this](const HttpRequest&)
    {
        return handleZapret();
    });

    server_.setFilter([this](const HttpRequest& request)
    {
        return authenticate(request);
    });
}

Status ApiServer::start(std::string_view address, std::uint16_t port)
{
    if (!routesRegistered_)
    {
        registerRoutes();
        routesRegistered_ = true;
    }
    return server_.start(address, port);
}

void ApiServer::stop()
{
    server_.stop();
}

Status ApiServer::startEphemeral()
{
    if (!routesRegistered_)
    {
        registerRoutes();
        routesRegistered_ = true;
    }
    return server_.startEphemeral("127.0.0.1");
}

bool ApiServer::isRunning() const
{
    return server_.isRunning();
}

std::uint16_t ApiServer::boundPort() const
{
    return server_.boundPort();
}

}
