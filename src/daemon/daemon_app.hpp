#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include <deimos/model/config.hpp>
#include <deimos/support/error.hpp>
#include <deimos/support/log.hpp>
#include <deimos/zapret/toolchain.hpp>

#include "api_server.hpp"
#include "config_store.hpp"
#include "engine.hpp"

namespace deimos
{

struct DaemonOptions
{
    std::string configPath;
    std::string listenOverride;
    int portOverride = 0;
    std::string logLevelOverride;
};

/// Wires configuration, logging, zapret detection, the engine and the API together.
class DaemonApp
{
public:
    explicit DaemonApp(DaemonOptions options);
    ~DaemonApp();

    DaemonApp(const DaemonApp&) = delete;
    DaemonApp& operator=(const DaemonApp&) = delete;

    /// Loads configuration, configures logging and detects zapret. No sockets yet.
    Status initialize();

    /// Starts the API server and, when configured, the engine.
    Status start();

    /// Blocks until requestStop() or \p externalStop reports true.
    void waitForStop(const std::function<bool()>& externalStop);

    void requestStop();

    /// Stops the engine and the API server.
    void shutdown();

    std::uint16_t port() const;
    std::string listenAddress() const;
    std::string tokenFile() const;

    DeimosConfig config() const;

private:
    Status applyConfig(const DeimosConfig& config);
    EngineContext buildContext(const DeimosConfig& config) const;
    void configureLogging(const DeimosConfig& config);
    void refreshZapret(const DeimosConfig& config);

    DaemonOptions options_;
    std::unique_ptr<ConfigStore> store_;
    std::unique_ptr<RuntimeEngine> engine_;
    std::unique_ptr<ApiServer> api_;
    std::shared_ptr<LogRingBuffer> logBuffer_;
    std::shared_ptr<FileSink> fileSink_;
    ToolPaths zapretPaths_;
    std::chrono::system_clock::time_point startedAt_{};
    std::atomic<bool> stopRequested_{false};
    bool apiStarted_ = false;
};

}
