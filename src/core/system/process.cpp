#include <deimos/system/process.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

#include <deimos/support/strings.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace deimos
{

struct Process::Impl
{
    std::atomic<bool> running{false};
    std::atomic<int> exitCode{-1};
    std::uint64_t pid = 0;

    mutable std::mutex mutex;
    OutputCallback onOutput;

#ifdef _WIN32
    HANDLE processHandle = nullptr;
    HANDLE jobHandle = nullptr;
    HANDLE stdoutRead = nullptr;
    HANDLE stderrRead = nullptr;
    std::thread stdoutThread;
    std::thread stderrThread;
#else
    pid_t childPid = -1;
    int stdoutRead = -1;
    int stderrRead = -1;
    std::thread stdoutThread;
    std::thread stderrThread;
#endif
};

#ifdef _WIN32
namespace
{

void appendQuoted(std::wstring& out, const std::wstring& argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        out.append(argument);
        return;
    }

    out.push_back(L'"');
    for (auto iterator = argument.begin();; ++iterator)
    {
        std::size_t backslashes = 0;
        while (iterator != argument.end() && *iterator == L'\\')
        {
            ++iterator;
            ++backslashes;
        }
        if (iterator == argument.end())
        {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (*iterator == L'"')
        {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            continue;
        }
        out.append(backslashes, L'\\');
        out.push_back(*iterator);
    }
    out.push_back(L'"');
}

std::wstring buildCommandLine(const ProcessOptions& options)
{
    std::wstring commandLine;
    appendQuoted(commandLine, strings::toWide(options.executable));
    for (const std::string& argument : options.arguments)
    {
        commandLine.push_back(L' ');
        appendQuoted(commandLine, strings::toWide(argument));
    }
    return commandLine;
}

bool createPipe(HANDLE& readEnd, HANDLE& writeEnd)
{
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    if (!CreatePipe(&readEnd, &writeEnd, &attributes, 0))
    {
        return false;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    return true;
}

void closeHandle(HANDLE& handle)
{
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
    }
    handle = nullptr;
}

} // namespace
#endif

Process::Process()
    : impl_(std::make_unique<Impl>())
{
}

Process::~Process()
{
    stop(std::chrono::milliseconds{1000});

#ifdef _WIN32
    if (impl_->stdoutThread.joinable())
    {
        impl_->stdoutThread.join();
    }
    if (impl_->stderrThread.joinable())
    {
        impl_->stderrThread.join();
    }
    closeHandle(impl_->stdoutRead);
    closeHandle(impl_->stderrRead);
    closeHandle(impl_->processHandle);
    closeHandle(impl_->jobHandle);
#else
    if (impl_->stdoutThread.joinable())
    {
        impl_->stdoutThread.join();
    }
    if (impl_->stderrThread.joinable())
    {
        impl_->stderrThread.join();
    }
    if (impl_->stdoutRead >= 0)
    {
        ::close(impl_->stdoutRead);
    }
    if (impl_->stderrRead >= 0)
    {
        ::close(impl_->stderrRead);
    }
#endif
}

#ifdef _WIN32

Status Process::start(const ProcessOptions& options, OutputCallback onOutput)
{
    if (impl_->running.load())
    {
        return Status::failure(ErrorCode::conflict, "process is already running");
    }

    impl_->onOutput = std::move(onOutput);

    HANDLE stdoutWrite = nullptr;
    HANDLE stderrWrite = nullptr;
    HANDLE stdoutRead = nullptr;
    HANDLE stderrRead = nullptr;

    if (!createPipe(stdoutRead, stdoutWrite) || !createPipe(stderrRead, stderrWrite))
    {
        closeHandle(stdoutRead);
        closeHandle(stdoutWrite);
        closeHandle(stderrRead);
        closeHandle(stderrWrite);
        return Status::failure(ErrorCode::internal, "cannot create pipes for the child process");
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = stdoutWrite;
    startupInfo.hStdError = stderrWrite;
    startupInfo.hStdInput = nullptr;

    PROCESS_INFORMATION processInfo{};
    const std::wstring wideCommandLine = buildCommandLine(options);
    std::vector<wchar_t> commandBuffer(wideCommandLine.begin(), wideCommandLine.end());
    commandBuffer.push_back(L'\0');

    const std::wstring wideExecutable = strings::toWide(options.executable);
    const std::wstring wideWorkingDirectory = strings::toWide(options.workingDirectory);

    DWORD creationFlags = CREATE_NO_WINDOW;
    if (impl_->jobHandle == nullptr && options.killOnParentExit)
    {
        impl_->jobHandle = CreateJobObjectW(nullptr, nullptr);
        if (impl_->jobHandle != nullptr)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInfo{};
            limitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(impl_->jobHandle,
                                    JobObjectExtendedLimitInformation,
                                    &limitInfo,
                                    sizeof(limitInfo));
        }
    }

    const std::filesystem::path executablePath{options.executable};
    const bool needsPathSearch = !executablePath.has_parent_path();

    const BOOL created = CreateProcessW(needsPathSearch ? nullptr : wideExecutable.c_str(),
                                        commandBuffer.data(),
                                        nullptr,
                                        nullptr,
                                        TRUE,
                                        creationFlags,
                                        nullptr,
                                        wideWorkingDirectory.empty() ? nullptr : wideWorkingDirectory.c_str(),
                                        &startupInfo,
                                        &processInfo);
    const DWORD creationError = created ? 0 : GetLastError();

    closeHandle(stdoutWrite);
    closeHandle(stderrWrite);

    if (!created)
    {
        closeHandle(stdoutRead);
        closeHandle(stderrRead);
        return Status::failure(ErrorCode::io,
                               "cannot start '" + options.executable + "'",
                               "windows error " + std::to_string(creationError));
    }

    if (impl_->jobHandle != nullptr)
    {
        AssignProcessToJobObject(impl_->jobHandle, processInfo.hProcess);
    }

    CloseHandle(processInfo.hThread);

    impl_->processHandle = processInfo.hProcess;
    impl_->pid = static_cast<std::uint64_t>(processInfo.dwProcessId);
    impl_->stdoutRead = stdoutRead;
    impl_->stderrRead = stderrRead;
    impl_->exitCode.store(-1);
    impl_->running.store(true);

    const auto pump = [this](HANDLE handle, bool isError)
    {
        char buffer[4096];
        for (;;)
        {
            DWORD read = 0;
            if (!ReadFile(handle, buffer, static_cast<DWORD>(sizeof(buffer)), &read, nullptr) || read == 0)
            {
                break;
            }
            std::lock_guard<std::mutex> lock(impl_->mutex);
            if (impl_->onOutput)
            {
                impl_->onOutput(std::string_view{buffer, read}, isError);
            }
        }
    };

    impl_->stdoutThread = std::thread{pump, stdoutRead, false};
    impl_->stderrThread = std::thread{pump, stderrRead, true};
    return Status::ok();
}

bool Process::waitForExit(std::chrono::milliseconds timeout)
{
    if (impl_->processHandle == nullptr)
    {
        return true;
    }
    const DWORD wait = WaitForSingleObject(impl_->processHandle, static_cast<DWORD>(timeout.count()));
    if (wait != WAIT_OBJECT_0)
    {
        return false;
    }
    DWORD code = 0;
    if (GetExitCodeProcess(impl_->processHandle, &code))
    {
        impl_->exitCode.store(static_cast<int>(code));
    }
    impl_->running.store(false);
    return true;
}

void Process::stop(std::chrono::milliseconds gracePeriod)
{
    if (impl_->processHandle == nullptr || !impl_->running.load())
    {
        return;
    }
    // zapret tools have no graceful shutdown protocol; give them a moment, then terminate.
    if (!waitForExit(gracePeriod))
    {
        TerminateProcess(impl_->processHandle, 1);
        waitForExit(std::chrono::milliseconds{2000});
    }
    impl_->running.store(false);
}

bool Process::poll()
{
    if (impl_->processHandle == nullptr)
    {
        return true;
    }
    if (!impl_->running.load())
    {
        return true;
    }
    if (WaitForSingleObject(impl_->processHandle, 0) != WAIT_OBJECT_0)
    {
        return false;
    }
    DWORD code = 0;
    if (GetExitCodeProcess(impl_->processHandle, &code))
    {
        impl_->exitCode.store(static_cast<int>(code));
    }
    impl_->running.store(false);
    return true;
}

bool Process::isRunning() const
{
    return impl_->running.load();
}

int Process::exitCode() const
{
    return impl_->exitCode.load();
}

std::uint64_t Process::pid() const
{
    return impl_->pid;
}

#else

Status Process::start(const ProcessOptions& options, OutputCallback onOutput)
{
    if (impl_->running.load())
    {
        return Status::failure(ErrorCode::conflict, "process is already running");
    }

    impl_->onOutput = std::move(onOutput);

    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};
    int execPipe[2] = {-1, -1};
    if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0 || pipe(execPipe) != 0)
    {
        return Status::failure(ErrorCode::internal, "cannot create pipes for the child process");
    }

    // The child reports a failed exec through execPipe. FD_CLOEXEC closes the write end
    // on a successful exec, so the parent sees end-of-file exactly when the child starts.
    fcntl(execPipe[1], F_SETFD, FD_CLOEXEC);

    const pid_t childPid = fork();
    if (childPid < 0)
    {
        return Status::failure(ErrorCode::internal, "cannot fork");
    }

    const auto reportExecFailure = [execPipe](int errorCode)
    {
        const ssize_t written = ::write(execPipe[1], &errorCode, sizeof(errorCode));
        (void)written;
    };

    if (childPid == 0)
    {
        close(execPipe[0]);
        setpgid(0, 0);
        if (!options.workingDirectory.empty() && chdir(options.workingDirectory.c_str()) != 0)
        {
            reportExecFailure(errno);
            _exit(126);
        }
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);

        std::vector<char*> argv;
        argv.reserve(options.arguments.size() + 2);
        argv.push_back(const_cast<char*>(options.executable.c_str()));
        for (const std::string& argument : options.arguments)
        {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);

        execvp(options.executable.c_str(), argv.data());
        reportExecFailure(errno);
        _exit(127);
    }

    setpgid(childPid, childPid);
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    close(execPipe[1]);

    int execError = 0;
    const ssize_t execRead = ::read(execPipe[0], &execError, sizeof(execError));
    close(execPipe[0]);
    if (execRead > 0)
    {
        int childStatus = 0;
        waitpid(childPid, &childStatus, 0);
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        return Status::failure(ErrorCode::io,
                               "cannot start '" + options.executable + "'",
                               std::strerror(execError));
    }

    impl_->childPid = childPid;
    impl_->pid = static_cast<std::uint64_t>(childPid);
    impl_->stdoutRead = stdoutPipe[0];
    impl_->stderrRead = stderrPipe[0];
    impl_->exitCode.store(-1);
    impl_->running.store(true);

    const auto pump = [this](int descriptor, bool isError)
    {
        char buffer[4096];
        for (;;)
        {
            const ssize_t read = ::read(descriptor, buffer, sizeof(buffer));
            if (read <= 0)
            {
                break;
            }
            std::lock_guard<std::mutex> lock(impl_->mutex);
            if (impl_->onOutput)
            {
                impl_->onOutput(std::string_view{buffer, static_cast<std::size_t>(read)}, isError);
            }
        }
    };

    impl_->stdoutThread = std::thread{pump, impl_->stdoutRead, false};
    impl_->stderrThread = std::thread{pump, impl_->stderrRead, true};
    return Status::ok();
}

bool Process::waitForExit(std::chrono::milliseconds timeout)
{
    if (impl_->childPid <= 0)
    {
        return true;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;)
    {
        int status = 0;
        const pid_t result = waitpid(impl_->childPid, &status, WNOHANG);
        if (result == impl_->childPid)
        {
            if (WIFEXITED(status))
            {
                impl_->exitCode.store(WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                impl_->exitCode.store(128 + WTERMSIG(status));
            }
            impl_->running.store(false);
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
}

void Process::stop(std::chrono::milliseconds gracePeriod)
{
    if (impl_->childPid <= 0 || !impl_->running.load())
    {
        return;
    }

    kill(-impl_->childPid, SIGTERM);
    if (!waitForExit(gracePeriod))
    {
        kill(-impl_->childPid, SIGKILL);
        waitForExit(std::chrono::milliseconds{2000});
    }
    impl_->running.store(false);
}

bool Process::poll()
{
    if (impl_->childPid <= 0)
    {
        return true;
    }
    if (!impl_->running.load())
    {
        return true;
    }

    int status = 0;
    const pid_t result = waitpid(impl_->childPid, &status, WNOHANG);
    if (result != impl_->childPid)
    {
        return false;
    }
    if (WIFEXITED(status))
    {
        impl_->exitCode.store(WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        impl_->exitCode.store(128 + WTERMSIG(status));
    }
    impl_->running.store(false);
    return true;
}

bool Process::isRunning() const
{
    return impl_->running.load();
}

int Process::exitCode() const
{
    return impl_->exitCode.load();
}

std::uint64_t Process::pid() const
{
    return impl_->pid;
}

#endif

}
