#include "service.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#include <deimos/model/config.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/i18n.hpp>
#include <deimos/support/strings.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace deimos::service
{
namespace
{

std::string resolveExecutable(const InstallOptions& options)
{
    if (!options.executablePath.empty())
    {
        return options.executablePath;
    }
    return platform::executablePath().string();
}

std::string daemonConfigurationPath(const InstallOptions& options)
{
    if (!options.configPath.empty())
    {
        return options.configPath;
    }
    return defaultConfigPath();
}

std::vector<std::string> serviceArguments(const InstallOptions& options)
{
    if (!options.arguments.empty())
    {
        return options.arguments;
    }
    return {"--service-run", "--config", daemonConfigurationPath(options)};
}

bool isPlausibleServiceName(const std::string& name)
{
    if (name.empty())
    {
        return false;
    }
    return std::all_of(name.begin(),
                       name.end(),
                       [](char character)
                       {
                           return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' ||
                                  character == '_' || character == '.';
                       });
}

/// Arguments without the Windows service flag, for systemd and launchd.
std::vector<std::string> foregroundArguments(const InstallOptions& options)
{
    std::vector<std::string> arguments;
    for (const std::string& argument : serviceArguments(options))
    {
        if (argument == "--service-run")
        {
            continue;
        }
        arguments.push_back(argument);
    }
    if (std::find(arguments.begin(), arguments.end(), "--foreground") == arguments.end())
    {
        arguments.insert(arguments.begin(), "--foreground");
    }
    return arguments;
}

} // namespace

std::string renderSystemdUnit(const InstallOptions& options)
{
    std::ostringstream stream;
    stream << "[Unit]\n";
    stream << "Description=" << options.displayName << "\n";
    stream << "Documentation=https://github.com/ToshiNamura/Deimos\n";
    stream << "After=network-online.target\n";
    stream << "Wants=network-online.target\n\n";
    stream << "[Service]\n";
    stream << "Type=simple\n";
    stream << "ExecStart=" << resolveExecutable(options);
    for (const std::string& argument : foregroundArguments(options))
    {
        stream << " " << argument;
    }
    stream << "\n";
    stream << "Restart=on-failure\n";
    stream << "RestartSec=3\n";
    stream << "# zapret needs raw packet access on most systems\n";
    stream << "AmbientCapabilities=CAP_NET_ADMIN CAP_NET_RAW\n";
    stream << "CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_RAW\n";
    stream << "NoNewPrivileges=yes\n";
    stream << "ProtectSystem=full\n";
    stream << "ProtectHome=read-only\n\n";
    stream << "[Install]\n";
    stream << "WantedBy=multi-user.target\n";
    return stream.str();
}

std::string renderLaunchdPlist(const InstallOptions& options)
{
    std::ostringstream stream;
    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    stream << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
              "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    stream << "<plist version=\"1.0\">\n<dict>\n";
    stream << "  <key>Label</key>\n  <string>com.deimos.daemon</string>\n";
    stream << "  <key>ProgramArguments</key>\n  <array>\n";
    stream << "    <string>" << resolveExecutable(options) << "</string>\n";
    for (const std::string& argument : foregroundArguments(options))
    {
        stream << "    <string>" << argument << "</string>\n";
    }
    stream << "  </array>\n";
    stream << "  <key>RunAtLoad</key>\n  <true/>\n";
    stream << "  <key>KeepAlive</key>\n  <true/>\n";
    stream << "  <key>StandardOutPath</key>\n  <string>/var/log/deimosd.log</string>\n";
    stream << "  <key>StandardErrorPath</key>\n  <string>/var/log/deimosd.log</string>\n";
    stream << "</dict>\n</plist>\n";
    return stream.str();
}

#ifdef _WIN32

namespace
{

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
std::wstring g_serviceName = L"deimosd";
std::function<int()> g_runBlocking;
std::function<void()> g_requestStop;

void WINAPI controlHandler(DWORD control)
{
    if (control != SERVICE_CONTROL_STOP && control != SERVICE_CONTROL_SHUTDOWN)
    {
        return;
    }
    g_status.dwCurrentState = SERVICE_STOP_PENDING;
    g_status.dwWaitHint = 20000;
    SetServiceStatus(g_statusHandle, &g_status);
    if (g_requestStop)
    {
        g_requestStop();
    }
}

void WINAPI serviceMain(DWORD, LPWSTR*)
{
    g_statusHandle = RegisterServiceCtrlHandlerW(g_serviceName.c_str(), controlHandler);
    if (g_statusHandle == nullptr)
    {
        return;
    }

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = SERVICE_RUNNING;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_status.dwWin32ExitCode = NO_ERROR;
    SetServiceStatus(g_statusHandle, &g_status);

    const int exitCode = g_runBlocking ? g_runBlocking() : 1;

    g_status.dwCurrentState = SERVICE_STOPPED;
    g_status.dwControlsAccepted = 0;
    if (exitCode == 0)
    {
        g_status.dwWin32ExitCode = NO_ERROR;
    }
    else
    {
        g_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        g_status.dwServiceSpecificExitCode = static_cast<DWORD>(exitCode);
    }
    SetServiceStatus(g_statusHandle, &g_status);
}

} // namespace

bool isRunningAsService()
{
    return GetConsoleWindow() == nullptr;
}

bool runAsService(const std::string& serviceName,
                  const std::function<int()>& runBlocking,
                  const std::function<void()>& requestStop,
                  std::string& errorMessage)
{
    g_serviceName = strings::toWide(serviceName);
    g_runBlocking = runBlocking;
    g_requestStop = requestStop;

    SERVICE_TABLE_ENTRYW dispatchTable[] = {
        {g_serviceName.data(), &serviceMain},
        {nullptr, nullptr},
    };

    if (!StartServiceCtrlDispatcherW(dispatchTable))
    {
        const DWORD lastError = GetLastError();
        errorMessage = lastError == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT
                           ? std::string{"not started by the service control manager"}
                           : "service dispatcher failed with error " + std::to_string(lastError);
        return false;
    }
    return true;
}

Status install(const InstallOptions& options, std::string& message)
{
    if (!isPlausibleServiceName(options.serviceName))
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.service.invalidName"), options.serviceName);
    }

    const std::string executable = resolveExecutable(options);
    if (executable.empty())
    {
        return Status::failure(ErrorCode::notFound, i18n::tr("err.service.resolveExecutable"));
    }

    std::string commandLine = "\"" + executable + "\"";
    for (const std::string& argument : serviceArguments(options))
    {
        commandLine += " \"" + argument + "\"";
    }

    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (manager == nullptr)
    {
        return Status::failure(ErrorCode::unauthorized,
                               "cannot open the service control manager",
                               "run the daemon from an elevated prompt (error " +
                                   std::to_string(GetLastError()) + ")");
    }

    const std::wstring wideName = strings::toWide(options.serviceName);
    const std::wstring wideDisplay = strings::toWide(options.displayName);
    const std::wstring wideCommand = strings::toWide(commandLine);

    SC_HANDLE serviceHandle = CreateServiceW(manager,
                                             wideName.c_str(),
                                             wideDisplay.c_str(),
                                             SERVICE_ALL_ACCESS,
                                             SERVICE_WIN32_OWN_PROCESS,
                                             SERVICE_AUTO_START,
                                             SERVICE_ERROR_NORMAL,
                                             wideCommand.c_str(),
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             nullptr);

    if (serviceHandle == nullptr)
    {
        const DWORD lastError = GetLastError();
        CloseServiceHandle(manager);
        if (lastError == ERROR_SERVICE_EXISTS)
        {
            return Status::failure(ErrorCode::alreadyExists, i18n::tr("err.service.alreadyInstalled"), options.serviceName);
        }
        return Status::failure(ErrorCode::io,
                               "cannot create the service",
                               "windows error " + std::to_string(lastError));
    }

    std::wstring wideDescription = strings::toWide(options.description);
    SERVICE_DESCRIPTIONW description{};
    description.lpDescription = wideDescription.data();
    ChangeServiceConfig2W(serviceHandle, SERVICE_CONFIG_DESCRIPTION, &description);

    if (options.startImmediately && !StartServiceW(serviceHandle, 0, nullptr))
    {
        const DWORD lastError = GetLastError();
        if (lastError != ERROR_SERVICE_ALREADY_RUNNING)
        {
            CloseServiceHandle(serviceHandle);
            CloseServiceHandle(manager);
            return Status::failure(ErrorCode::io,
                                   "the service is installed but could not be started",
                                   "windows error " + std::to_string(lastError));
        }
    }

    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(manager);
    message = "service '" + options.serviceName + "' installed; inspect it with 'sc query " + options.serviceName + "'";
    return Status::ok();
}

Status uninstall(const std::string& serviceName, std::string& message)
{
    if (!isPlausibleServiceName(serviceName))
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.service.invalidName"), serviceName);
    }

    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (manager == nullptr)
    {
        return Status::failure(ErrorCode::unauthorized, i18n::tr("err.service.openManager"));
    }

    const std::wstring wideName = strings::toWide(serviceName);
    SC_HANDLE serviceHandle = OpenServiceW(manager, wideName.c_str(), SERVICE_STOP | DELETE);
    if (serviceHandle == nullptr)
    {
        const DWORD lastError = GetLastError();
        CloseServiceHandle(manager);
        if (lastError == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            return Status::failure(ErrorCode::notFound, i18n::tr("err.service.notInstalled"), serviceName);
        }
        return Status::failure(ErrorCode::io, i18n::tr("err.service.openService"), "windows error " + std::to_string(lastError));
    }

    SERVICE_STATUS status{};
    ControlService(serviceHandle, SERVICE_CONTROL_STOP, &status);
    const bool deleted = DeleteService(serviceHandle) != 0;
    const DWORD lastError = deleted ? 0 : GetLastError();

    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(manager);

    if (!deleted)
    {
        return Status::failure(ErrorCode::io, i18n::tr("err.service.deleteService"), "windows error " + std::to_string(lastError));
    }

    message = "service '" + serviceName + "' removed";
    return Status::ok();
}

#else

static int runCommand(const std::string& command)
{
    return std::system(command.c_str());
}

bool isRunningAsService()
{
    return false;
}

bool runAsService(const std::string&,
                  const std::function<int()>&,
                  const std::function<void()>&,
                  std::string& errorMessage)
{
    errorMessage = "native service dispatch is only available on Windows; use systemd or launchd";
    return false;
}

Status install(const InstallOptions& options, std::string& message)
{
    if (!isPlausibleServiceName(options.serviceName))
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.service.invalidName"), options.serviceName);
    }

#if defined(__APPLE__)
    const std::string path = "/Library/LaunchDaemons/com.deimos.daemon.plist";
    const std::string content = renderLaunchdPlist(options);
#else
    const std::string path = "/etc/systemd/system/" + options.serviceName + ".service";
    const std::string content = renderSystemdUnit(options);
#endif

    if (!platform::isElevated())
    {
        message = "installation requires root. Write this file to " + path + ":\n\n" + content;
        return Status::failure(ErrorCode::unauthorized, i18n::tr("err.service.rootRequired"), path);
    }

    std::string errorMessage;
    if (!platform::writeFile(path, content, errorMessage))
    {
        return Status::failure(ErrorCode::io, i18n::tr("err.service.writeDefinition"), errorMessage);
    }

#if defined(__APPLE__)
    const std::string registerCommand = "launchctl load -w " + path;
    const std::string startCommand = "launchctl start com.deimos.daemon";
#else
    const std::string registerCommand =
        "systemctl daemon-reload && systemctl enable " + options.serviceName + " >/dev/null 2>&1";
    const std::string startCommand = "systemctl start " + options.serviceName;
#endif

    if (runCommand(registerCommand) != 0)
    {
        message = "service definition written to " + path + " but the init system reported an error";
        return Status::failure(ErrorCode::io, i18n::tr("err.service.register"), registerCommand);
    }

    if (options.startImmediately)
    {
        runCommand(startCommand);
    }

    message = "service installed at " + path;
    return Status::ok();
}

Status uninstall(const std::string& serviceName, std::string& message)
{
    if (!isPlausibleServiceName(serviceName))
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.service.invalidName"), serviceName);
    }
    if (!platform::isElevated())
    {
        return Status::failure(ErrorCode::unauthorized, i18n::tr("err.service.rootRequiredRemove"));
    }

#if defined(__APPLE__)
    runCommand("launchctl unload -w /Library/LaunchDaemons/com.deimos.daemon.plist 2>/dev/null");
    std::error_code code;
    std::filesystem::remove("/Library/LaunchDaemons/com.deimos.daemon.plist", code);
#else
    runCommand(std::string{"systemctl disable --now "} + serviceName + " >/dev/null 2>&1");
    std::error_code code;
    std::filesystem::remove("/etc/systemd/system/" + serviceName + ".service", code);
    runCommand("systemctl daemon-reload");
#endif

    message = "service '" + serviceName + "' removed";
    return Status::ok();
}

#endif

}
