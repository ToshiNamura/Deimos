#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/model/profile.hpp>
#include <deimos/support/error.hpp>
#include <deimos/system/process.hpp>
#include <deimos/transport/protocol.hpp>
#include <deimos/zapret/command_builder.hpp>
#include <deimos/zapret/toolchain.hpp>

namespace deimos
{

/// Everything the engine needs from the configuration, captured in one immutable value.
struct EngineContext
{
    EngineSettings settings;
    ZapretSettings zapret;
    ToolPaths paths;
    std::string workingDirectory;
};

/// Supervises the zapret child processes for one active profile.
class RuntimeEngine
{
public:
    explicit RuntimeEngine(EngineContext context);
    ~RuntimeEngine();

    RuntimeEngine(const RuntimeEngine&) = delete;
    RuntimeEngine& operator=(const RuntimeEngine&) = delete;

    /// Replaces the context. Only allowed while the engine is stopped.
    Status updateContext(const EngineContext& context);

    Result<EngineSnapshot> start(const Profile& profile);
    Status stop();
    Result<EngineSnapshot> restart(const Profile& profile);

    EngineSnapshot snapshot() const;
    bool isRunning() const;

    /// Toolchain currently in use, exposed through the status endpoint.
    ToolPaths toolPaths() const;

private:
    Status spawnLocked(const std::vector<LaunchSpec>& launches);
    void shutdownProcessesLocked();
    void monitorLoop();
    void handleOutput(const std::string& label, std::string_view chunk, bool isErrorStream);
    EngineSnapshot snapshotLocked() const;

    mutable std::mutex mutex_;
    mutable std::mutex outputMutex_;
    EngineContext context_;
    EngineState state_ = EngineState::stopped;
    std::string activeProfileId_;
    std::string lastError_;
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::unique_ptr<Process>> processes_;
    std::vector<LaunchSpec> currentLaunches_;
    std::vector<LaunchInfo> launches_;
    std::map<std::string, std::string> partialOutput_;
    std::chrono::system_clock::time_point startedAt_{};
    std::chrono::steady_clock::time_point steadyStartedAt_{};
    int restartCount_ = 0;
    bool hasStarted_ = false;
    std::atomic<bool> stopping_{false};
    std::thread monitor_;
    bool monitorRunning_ = false;
};

}
