#include "engine.hpp"

#include <utility>

#include <deimos/support/log.hpp>
#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

std::string profileLabel(const Profile& profile)
{
    return profile.id.empty() ? std::string{"<unnamed>"} : profile.id;
}

} // namespace

RuntimeEngine::RuntimeEngine(EngineContext context)
    : context_(std::move(context))
{
}

RuntimeEngine::~RuntimeEngine()
{
    stop();
}

Status RuntimeEngine::updateContext(const EngineContext& context)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != EngineState::stopped)
    {
        return Status::failure(ErrorCode::conflict, "stop the engine before changing its context");
    }
    context_ = context;
    return Status::ok();
}

Result<EngineSnapshot> RuntimeEngine::start(const Profile& profile)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != EngineState::stopped && state_ != EngineState::faulted)
    {
        return Result<EngineSnapshot>{
            Error{ErrorCode::conflict, "engine is already " + std::string{toString(state_)}}};
    }

    ToolKind tool = profile.tool;
    if (tool == ToolKind::autoDetect)
    {
        const auto resolved = resolveTool(ToolKind::autoDetect, context_.paths);
        tool = resolved.hasValue() ? resolved.value() : platformDefaultTool();
    }
    else if (!context_.paths.has(tool))
    {
        return Result<EngineSnapshot>{
            Error{ErrorCode::notFound,
                  "zapret '" + std::string{toCliToken(tool)} + "' is not available on this daemon",
                  "install zapret or point zapret.installDirectory at it"}};
    }

    const std::string workingDirectory =
        context_.workingDirectory.empty() ? context_.paths.baseDirectory.string() : context_.workingDirectory;

    BuildResult build = buildLaunches(profile, tool, context_.paths, workingDirectory);
    diagnostics_ = build.diagnostics;
    if (build.hasErrors())
    {
        state_ = EngineState::faulted;
        lastError_ = "profile " + profileLabel(profile) + " cannot be started";
        const std::string report = formatDiagnostics(build.diagnostics);
        log::error("engine", lastError_ + "\n" + report);
        return Result<EngineSnapshot>{
            Error{ErrorCode::configuration, lastError_, strings::trim(report)}};
    }

    const Status spawnStatus = spawnLocked(build.launches);
    if (!spawnStatus.isOk())
    {
        shutdownProcessesLocked();
        state_ = EngineState::faulted;
        lastError_ = spawnStatus.error().describe();
        return Result<EngineSnapshot>{spawnStatus.error()};
    }

    currentLaunches_ = build.launches;
    activeProfileId_ = profile.id;
    restartCount_ = 0;
    startedAt_ = std::chrono::system_clock::now();
    steadyStartedAt_ = std::chrono::steady_clock::now();
    state_ = EngineState::running;
    lastError_.clear();
    stopping_.store(false);
    hasStarted_ = true;

    if (monitor_.joinable())
    {
        monitor_.join();
    }
    monitorRunning_ = true;
    monitor_ = std::thread(&RuntimeEngine::monitorLoop, this);

    log::info("engine",
              "profile '" + profileLabel(profile) + "' started with " + std::to_string(build.launches.size()) +
                  " process(es) using " + std::string{toCliToken(tool)});
    return Result<EngineSnapshot>{snapshotLocked()};
}

Status RuntimeEngine::stop()
{
    std::thread monitor;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == EngineState::stopped && !monitorRunning_)
        {
            return Status::ok();
        }
        state_ = EngineState::stopping;
        stopping_.store(true);
        shutdownProcessesLocked();
        if (monitor_.joinable())
        {
            monitor = std::move(monitor_);
        }
    }

    if (monitor.joinable())
    {
        monitor.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    monitorRunning_ = false;
    stopping_.store(false);
    state_ = EngineState::stopped;
    launches_.clear();
    currentLaunches_.clear();
    {
        std::lock_guard<std::mutex> outputLock(outputMutex_);
        partialOutput_.clear();
    }
    log::info("engine", "engine stopped");
    return Status::ok();
}

Result<EngineSnapshot> RuntimeEngine::restart(const Profile& profile)
{
    const Status stopStatus = stop();
    if (!stopStatus.isOk())
    {
        return Result<EngineSnapshot>{stopStatus.error()};
    }
    return start(profile);
}

EngineSnapshot RuntimeEngine::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshotLocked();
}

bool RuntimeEngine::isRunning() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == EngineState::running;
}

ToolPaths RuntimeEngine::toolPaths() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return context_.paths;
}

EngineSnapshot RuntimeEngine::snapshotLocked() const
{
    EngineSnapshot snapshot;
    snapshot.state = state_;
    snapshot.activeProfileId = activeProfileId_;
    snapshot.restartCount = restartCount_;
    snapshot.lastError = lastError_;
    snapshot.diagnostics = diagnostics_;
    snapshot.launches = launches_;

    if (state_ == EngineState::running && hasStarted_)
    {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - steadyStartedAt_);
        snapshot.uptimeSeconds = elapsed.count();
    }
    return snapshot;
}

Status RuntimeEngine::spawnLocked(const std::vector<LaunchSpec>& launches)
{
    launches_.clear();
    processes_.clear();

    for (const LaunchSpec& launch : launches)
    {
        auto process = std::make_unique<Process>();
        ProcessOptions options;
        options.executable = launch.executable;
        options.arguments = launch.arguments;
        options.workingDirectory = launch.workingDirectory;
        options.captureOutput = true;
        options.killOnParentExit = true;

        const std::string label = launch.label;
        const Status status = process->start(options,
                                             [this, label](std::string_view chunk, bool isErrorStream)
                                             {
                                                 handleOutput(label, chunk, isErrorStream);
                                             });
        if (!status.isOk())
        {
            return status;
        }

        LaunchInfo info;
        info.label = launch.label;
        info.command = launch.renderCommandLine();
        info.pid = process->pid();
        launches_.push_back(std::move(info));
        processes_.push_back(std::move(process));

        log::info("engine", "spawned " + launch.renderCommandLine());
    }

    return Status::ok();
}

void RuntimeEngine::shutdownProcessesLocked()
{
    for (auto& process : processes_)
    {
        if (process)
        {
            process->stop(std::chrono::milliseconds{context_.settings.stopTimeoutMs});
        }
    }
    processes_.clear();
    launches_.clear();
}

void RuntimeEngine::handleOutput(const std::string& label, std::string_view chunk, bool isErrorStream)
{
    std::lock_guard<std::mutex> lock(outputMutex_);
    std::string& buffer = partialOutput_[label];
    buffer.append(chunk);

    std::size_t position = 0;
    while ((position = buffer.find('\n')) != std::string::npos)
    {
        std::string line = buffer.substr(0, position);
        buffer.erase(0, position + 1);
        line = strings::trim(line);
        if (line.empty())
        {
            continue;
        }
        if (isErrorStream)
        {
            log::warning("zapret/" + label, std::move(line));
        }
        else
        {
            log::info("zapret/" + label, std::move(line));
        }
    }

    if (buffer.size() > 8192)
    {
        buffer.clear();
    }
}

void RuntimeEngine::monitorLoop()
{
    for (;;)
    {
        for (int tick = 0; tick < 10 && !stopping_.load(); ++tick)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
        if (stopping_.load())
        {
            return;
        }

        std::string failedLabel;
        int failedCode = 0;
        bool anyExited = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (std::size_t index = 0; index < processes_.size(); ++index)
            {
                if (!processes_[index] || !processes_[index]->poll())
                {
                    continue;
                }
                anyExited = true;
                failedLabel = index < launches_.size() ? launches_[index].label : std::string{"zapret"};
                failedCode = processes_[index]->exitCode();
                processes_[index].reset();
            }
        }

        if (!anyExited)
        {
            continue;
        }

        EngineSettings settings;
        bool canRestart = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            settings = context_.settings;
            canRestart = settings.restartOnCrash && restartCount_ < settings.maxRestarts &&
                         state_ == EngineState::running;
        }

        if (!canRestart)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastError_ =
                "zapret process '" + failedLabel + "' exited with code " + std::to_string(failedCode);
            state_ = EngineState::faulted;
            log::error("engine", lastError_);
            shutdownProcessesLocked();
            return;
        }

        int attempt = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            attempt = ++restartCount_;
        }
        log::warning("engine",
                     "zapret process '" + failedLabel + "' exited with code " + std::to_string(failedCode) +
                         "; restarting (attempt " + std::to_string(attempt) + " of " +
                         std::to_string(settings.maxRestarts) + ")");

        for (int tick = 0; tick < settings.restartBackoffMs / 50 + 1 && !stopping_.load(); ++tick)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
        if (stopping_.load())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != EngineState::running)
        {
            return;
        }
        if (const Status status = spawnLocked(currentLaunches_); !status.isOk())
        {
            lastError_ = status.error().describe();
            state_ = EngineState::faulted;
            log::error("engine", "restart failed: " + lastError_);
            shutdownProcessesLocked();
            return;
        }
    }
}

}
