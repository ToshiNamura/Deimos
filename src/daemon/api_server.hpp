#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/support/log.hpp>
#include <deimos/transport/http_server.hpp>

#include "config_store.hpp"
#include "engine.hpp"

namespace deimos
{

struct ApiServerDependencies
{
    ConfigStore* configStore = nullptr;
    RuntimeEngine* engine = nullptr;
    std::shared_ptr<LogRingBuffer> logs;
    std::string version;
    std::chrono::system_clock::time_point startedAt{};

    /// Lets the daemon react to a configuration change (logging, zapret re-detection, engine restart).
    std::function<Status(const DeimosConfig&)> onConfigChanged;
};

/// REST surface described in docs/daemon-api.md.
class ApiServer
{
public:
    explicit ApiServer(ApiServerDependencies dependencies);
    ~ApiServer();

    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;

    Status start(std::string_view address, std::uint16_t port);

    /// Binds an ephemeral port. Used by tests.
    Status startEphemeral();
    void stop();

    bool isRunning() const;
    std::uint16_t boundPort() const;

    /// Token required by the API, empty when authentication is disabled.
    void setToken(std::string token);

private:
    void registerRoutes();
    std::optional<HttpResponse> authenticate(const HttpRequest& request) const;
    HttpResponse handleStatus() const;
    HttpResponse handleStartEngine(const HttpRequest& request);
    HttpResponse handleStopEngine();
    HttpResponse handleRestartEngine(const HttpRequest& request);
    HttpResponse handleListProfiles() const;
    HttpResponse handleGetProfile(const HttpRequest& request) const;
    HttpResponse handleSaveProfile(const HttpRequest& request);
    HttpResponse handleDeleteProfile(const HttpRequest& request);
    HttpResponse handleActivateProfile(const HttpRequest& request);
    HttpResponse handlePreviewProfile(const HttpRequest& request) const;
    HttpResponse handleGetConfig() const;
    HttpResponse handlePutConfig(const HttpRequest& request);
    HttpResponse handleLogs(const HttpRequest& request) const;
    HttpResponse handleZapret() const;

    ApiServerDependencies dependencies_;
    HttpServer server_;
    bool routesRegistered_ = false;
    mutable std::mutex tokenMutex_;
    std::string token_;
};

}
