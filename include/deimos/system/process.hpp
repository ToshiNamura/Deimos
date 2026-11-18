#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/support/error.hpp>

namespace deimos
{

struct ProcessOptions
{
    std::string executable;
    std::vector<std::string> arguments;
    std::string workingDirectory;

    /// Capture stdout/stderr through the output callback instead of the parent console.
    bool captureOutput = true;

    /// Terminate the child together with the daemon (job object on Windows, process group on POSIX).
    bool killOnParentExit = true;
};

/// Minimal portable child process wrapper used to supervise zapret tools.
///
/// The output callback is invoked from reader threads but serialised internally,
/// so a caller never sees two concurrent callbacks.
class Process
{
public:
    using OutputCallback = std::function<void(std::string_view chunk, bool isErrorStream)>;

    Process();
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) = delete;
    Process& operator=(Process&&) = delete;

    /// Spawns the process. On failure the returned status carries the OS error text.
    Status start(const ProcessOptions& options, OutputCallback onOutput);

    /// Blocks until the child exits or the timeout elapses. Returns true when it exited.
    bool waitForExit(std::chrono::milliseconds timeout);

    /// Non-blocking check that refreshes the running state. Returns true when the child has exited.
    bool poll();

    /// Terminates the child. POSIX gets SIGTERM first, then SIGKILL after the grace period.
    void stop(std::chrono::milliseconds gracePeriod);

    bool isRunning() const;
    int exitCode() const;
    std::uint64_t pid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
