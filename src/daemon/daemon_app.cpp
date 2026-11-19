#include "daemon_app.hpp"

#include <thread>
#include <utility>

#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>
#include <deimos/version.hpp>

namespace deimos
{

DaemonApp::DaemonApp(DaemonOptions options)
    : options_(std::move(options))
{
}

DaemonApp::~DaemonApp()
{
    shutdown();
}

Status DaemonApp::initialize()
{
    std::string configPath = options_.configPath;
    if (configPath.empty())
    {
        configPath = defaultConfigPath();
    }

    store_ = std::make_unique<ConfigStore>(configPath);

    const bool configFileMissing = !store_->configFileExists();
    std::vector<Diagnostic> diagnostics;
    if (const Status status = store_->load(&diagnostics); !status.isOk())
    {
        for (const Diagnostic& diagnostic : diagnostics)
        {
            log::error("config", std::string{toString(diagnostic.severity)} + ": " + diagnostic.message);
        }
        return status;
    }

    // Only a real daemon start bootstraps a missing configuration file on disk.
    // Read-only commands (e.g. --validate-config) call ConfigStore::load() directly.
    if (configFileMissing)
    {
        if (const Status status = store_->save(&diagnostics); !status.isOk())
        {
            for (const Diagnostic& diagnostic : diagnostics)
            {
                log::error("config", std::string{toString(diagnostic.severity)} + ": " + diagnostic.message);
            }
            return status;
        }
        log::info("config", "wrote default configuration to " + configPath);
    }

    DeimosConfig config = store_->config();
    if (!options_.logLevelOverride.empty())
    {
        LogLevel level = config.daemon.logLevel;
        if (parseLogLevel(options_.logLevelOverride, level))
        {
            config.daemon.logLevel = level;
        }
    }

    configureLogging(config);
    refreshZapret(config);

    EngineContext context = buildContext(config);
    engine_ = std::make_unique<RuntimeEngine>(std::move(context));

    log::info("daemon", std::string{"Deimos daemon "} + versionString() + " on " + std::string{platform::platformName()} +
                            "/" + std::string{platform::architectureName()});
    log::info("config", "configuration file: " + configPath);
    return Status::ok();
}

void DaemonApp::configureLogging(const DeimosConfig& config)
{
    Logger& logger = Logger::global();
    logger.clearSinks();
    logger.setLevel(config.daemon.logLevel);

    auto console = std::make_shared<ConsoleSink>();
    logger.addSink(console);

    logBuffer_ = std::make_shared<LogRingBuffer>(config.daemon.logHistorySize);
    logger.addSink(logBuffer_);

    if (!fileSink_)
    {
        platform::ensureDirectory(platform::stateDirectory());
        fileSink_ = std::make_shared<FileSink>((platform::stateDirectory() / "deimosd.log").string());
    }
    logger.addSink(fileSink_);
}

void DaemonApp::refreshZapret(const DeimosConfig& config)
{
    const ToolchainReport report = detectZapret(config.zapret);
    zapretPaths_ = report.paths;

    for (const Diagnostic& diagnostic : report.diagnostics)
    {
        const std::string message = diagnostic.message;
        if (diagnostic.severity == Severity::error)
        {
            log::error("zapret", message);
        }
        else if (diagnostic.severity == Severity::warning)
        {
            log::warning("zapret", message);
        }
        else
        {
            log::info("zapret", message);
        }
    }
}

EngineContext DaemonApp::buildContext(const DeimosConfig& config) const
{
    EngineContext context;
    context.settings = config.engine;
    context.zapret = config.zapret;
    context.paths = zapretPaths_;
    context.workingDirectory =
        config.zapret.workingDirectory.empty() ? zapretPaths_.baseDirectory.string() : config.zapret.workingDirectory;
    return context;
}

Status DaemonApp::start()
{
    if (!store_ || !engine_)
    {
        return Status::failure(ErrorCode::conflict, "call initialize() before start()");
    }

    const DeimosConfig config = store_->config();

    const std::string address =
        options_.listenOverride.empty() ? config.daemon.listenAddress : options_.listenOverride;
    const std::uint16_t port =
        options_.portOverride > 0 ? static_cast<std::uint16_t>(options_.portOverride) : config.daemon.port;

    const auto token = store_->ensureToken();
    if (!token)
    {
        return Status{token.error()};
    }

    startedAt_ = std::chrono::system_clock::now();

    ApiServerDependencies dependencies;
    dependencies.configStore = store_.get();
    dependencies.engine = engine_.get();
    dependencies.logs = logBuffer_;
    dependencies.version = versionString();
    dependencies.startedAt = startedAt_;
    dependencies.onConfigChanged = [this](const DeimosConfig& updated)
    {
        return applyConfig(updated);
    };

    api_ = std::make_unique<ApiServer>(std::move(dependencies));
    api_->setToken(token.value());

    if (const Status status = api_->start(address, port); !status.isOk())
    {
        log::error("api", "cannot start the API server: " + status.error().describe());
        return status;
    }
    apiStarted_ = true;

    log::info("api",
              "listening on http://" + address + ":" + std::to_string(api_->boundPort()) +
                  (config.daemon.requireAuthentication ? " (bearer token required)" : " (authentication disabled)"));
    if (config.daemon.requireAuthentication)
    {
        log::info("api", "token file: " + store_->tokenFile());
    }

    if (config.daemon.autoStartEngine && !config.daemon.activeProfileId.empty())
    {
        if (const Profile* profile = config.findProfile(config.daemon.activeProfileId); profile != nullptr)
        {
            const auto result = engine_->start(*profile);
            if (!result)
            {
                log::error("engine",
                           "autostart of profile '" + config.daemon.activeProfileId +
                               "' failed: " + result.error().describe());
            }
        }
        else
        {
            log::warning("engine",
                         "autostart skipped: profile '" + config.daemon.activeProfileId + "' does not exist");
        }
    }

    return Status::ok();
}

void DaemonApp::waitForStop(const std::function<bool()>& externalStop)
{
    while (!stopRequested_.load())
    {
        for (int tick = 0; tick < 10 && !stopRequested_.load(); ++tick)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
        if (externalStop && externalStop())
        {
            break;
        }
    }
}

void DaemonApp::requestStop()
{
    stopRequested_.store(true);
}

void DaemonApp::shutdown()
{
    if (engine_)
    {
        engine_->stop();
    }
    if (api_ && apiStarted_)
    {
        api_->stop();
        apiStarted_ = false;
        log::info("api", "API server stopped");
    }
}

Status DaemonApp::applyConfig(const DeimosConfig& config)
{
    configureLogging(config);
    refreshZapret(config);

    if (!engine_)
    {
        return Status::ok();
    }

    const bool wasRunning = engine_->isRunning();
    const std::string previousProfile = engine_->snapshot().activeProfileId;
    if (wasRunning)
    {
        engine_->stop();
    }

    if (const Status status = engine_->updateContext(buildContext(config)); !status.isOk())
    {
        return status;
    }

    if (wasRunning)
    {
        const std::string profileId =
            config.daemon.activeProfileId.empty() ? previousProfile : config.daemon.activeProfileId;
        if (const Profile* profile = config.findProfile(profileId); profile != nullptr)
        {
            const auto result = engine_->start(*profile);
            if (!result)
            {
                log::error("engine", "restart after configuration change failed: " + result.error().describe());
            }
        }
    }

    log::info("config", "configuration applied");
    return Status::ok();
}

std::uint16_t DaemonApp::port() const
{
    return api_ ? api_->boundPort() : 0;
}

std::string DaemonApp::listenAddress() const
{
    return store_ ? store_->config().daemon.listenAddress : std::string{};
}

std::string DaemonApp::tokenFile() const
{
    return store_ ? store_->tokenFile() : std::string{};
}

DeimosConfig DaemonApp::config() const
{
    return store_ ? store_->config() : DeimosConfig{};
}

}
