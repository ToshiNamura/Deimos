#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "config_store.hpp"
#include "daemon_app.hpp"
#include "service.hpp"

#include <deimos/model/config.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>
#include <deimos/version.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

namespace
{

std::atomic<bool> g_consoleStop{false};

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD eventType)
{
    switch (eventType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_consoleStop.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}
#else
extern "C" void signalHandler(int)
{
    g_consoleStop.store(true);
}
#endif

void printUsage()
{
    std::cout << R"(Deimos daemon - edits outgoing traffic with zapret under remote control

Usage: deimosd [options]

Options:
  --config PATH            Configuration file (default: platform config directory)
  --listen ADDRESS         Override daemon.listenAddress
  --port PORT              Override daemon.port
  --log-level LEVEL        Override daemon.logLevel (trace|debug|info|warn|error)
  --foreground             Stay attached to the console (default when not a service)
  --service-run            Internal flag used by the platform service definition
  --service-name NAME      Service name for install/uninstall (default: deimosd)
  --install-service        Install and start the platform service
  --uninstall-service      Remove the platform service
  --print-unit             Print the systemd unit to stdout and exit
  --print-plist            Print the launchd plist to stdout and exit
  --generate-token         Create a new API token, print it once and exit
  --token-file PATH        Token file used with --generate-token
  --validate-config        Validate the configuration and exit
  --version                Print the version and exit
  --help                   Print this help and exit
)";
}

struct CommandLine
{
    deimos::DaemonOptions daemon;
    bool help = false;
    bool version = false;
    bool printUnit = false;
    bool printPlist = false;
    bool installService = false;
    bool uninstallService = false;
    bool generateToken = false;
    bool validateConfig = false;
    bool serviceRun = false;
    std::string serviceName{"deimosd"};
    std::string tokenFile;
    bool valid = true;
};

CommandLine parseCommandLine(int argc, char** argv)
{
    CommandLine parsed;

    for (int index = 1; index < argc; ++index)
    {
        std::string argument = argv[index];
        std::string inlineValue;
        if (const std::size_t equals = argument.find('='); equals != std::string::npos)
        {
            inlineValue = argument.substr(equals + 1);
            argument = argument.substr(0, equals);
        }

        const auto value = [&](std::string& target) -> bool
        {
            if (!inlineValue.empty())
            {
                target = inlineValue;
                return true;
            }
            if (index + 1 < argc)
            {
                target = argv[++index];
                return true;
            }
            std::cerr << "deimosd: missing value for " << argument << "\n";
            parsed.valid = false;
            return false;
        };

        if (argument == "--config")
        {
            value(parsed.daemon.configPath);
        }
        else if (argument == "--listen")
        {
            value(parsed.daemon.listenOverride);
        }
        else if (argument == "--port")
        {
            std::string text;
            if (value(text))
            {
                std::int64_t port = 0;
                if (deimos::strings::parseInt(text, port) && port > 0 && port <= 65535)
                {
                    parsed.daemon.portOverride = static_cast<int>(port);
                }
                else
                {
                    std::cerr << "deimosd: invalid port '" << text << "'\n";
                    parsed.valid = false;
                }
            }
        }
        else if (argument == "--log-level")
        {
            value(parsed.daemon.logLevelOverride);
        }
        else if (argument == "--service-name")
        {
            value(parsed.serviceName);
        }
        else if (argument == "--token-file")
        {
            value(parsed.tokenFile);
        }
        else if (argument == "--foreground")
        {
            // Foreground is the default; accepted for symmetry with service definitions.
        }
        else if (argument == "--service-run")
        {
            parsed.serviceRun = true;
        }
        else if (argument == "--install-service")
        {
            parsed.installService = true;
        }
        else if (argument == "--uninstall-service")
        {
            parsed.uninstallService = true;
        }
        else if (argument == "--print-unit")
        {
            parsed.printUnit = true;
        }
        else if (argument == "--print-plist")
        {
            parsed.printPlist = true;
        }
        else if (argument == "--generate-token")
        {
            parsed.generateToken = true;
        }
        else if (argument == "--validate-config")
        {
            parsed.validateConfig = true;
        }
        else if (argument == "--version")
        {
            parsed.version = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            parsed.help = true;
        }
        else
        {
            std::cerr << "deimosd: unknown option '" << argument << "'\n";
            parsed.valid = false;
        }
    }

    return parsed;
}

deimos::service::InstallOptions installOptionsFrom(const CommandLine& commandLine)
{
    deimos::service::InstallOptions options;
    options.serviceName = commandLine.serviceName;
    options.executablePath = deimos::platform::executablePath().string();
    options.configPath = commandLine.daemon.configPath.empty() ? deimos::defaultConfigPath()
                                                              : commandLine.daemon.configPath;
    options.arguments = {"--service-run", "--config", options.configPath};
    return options;
}

int runDaemon(const CommandLine& commandLine, bool serviceMode)
{
    deimos::DaemonApp app(commandLine.daemon);
    if (const deimos::Status status = app.initialize(); !status.isOk())
    {
        std::cerr << "deimosd: " << status.error().describe() << "\n";
        return 1;
    }

    if (const deimos::Status status = app.start(); !status.isOk())
    {
        std::cerr << "deimosd: " << status.error().describe() << "\n";
        app.shutdown();
        return 1;
    }

    if (serviceMode)
    {
        app.waitForStop([] { return false; });
    }
    else
    {
        std::cout << "deimosd is running. Press Ctrl+C to stop.\n";
        app.waitForStop([] { return g_consoleStop.load(); });
    }

    app.shutdown();
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN);
#endif

    const CommandLine commandLine = parseCommandLine(argc, argv);
    if (!commandLine.valid)
    {
        printUsage();
        return 2;
    }

    if (commandLine.help)
    {
        printUsage();
        return 0;
    }

    if (commandLine.version)
    {
        std::cout << "deimosd " << deimos::versionString() << " (" << deimos::platform::platformName() << "/"
                  << deimos::platform::architectureName() << ")\n";
        return 0;
    }

    if (commandLine.printUnit)
    {
        std::cout << deimos::service::renderSystemdUnit(installOptionsFrom(commandLine));
        return 0;
    }

    if (commandLine.printPlist)
    {
        std::cout << deimos::service::renderLaunchdPlist(installOptionsFrom(commandLine));
        return 0;
    }

    if (commandLine.generateToken)
    {
        deimos::ConfigStore store(commandLine.daemon.configPath.empty() ? deimos::defaultConfigPath()
                                                                       : commandLine.daemon.configPath);
        if (const deimos::Status status = store.load(); !status.isOk())
        {
            std::cerr << "deimosd: " << status.error().describe() << "\n";
            return 1;
        }

        const auto token = store.rotateToken();
        if (!token)
        {
            std::cerr << "deimosd: " << token.error().describe() << "\n";
            return 1;
        }
        std::cout << token.value() << "\n";
        std::cerr << "token written to " << store.tokenFile() << "\n";
        return 0;
    }

    if (commandLine.validateConfig)
    {
        deimos::ConfigStore store(commandLine.daemon.configPath.empty() ? deimos::defaultConfigPath()
                                                                       : commandLine.daemon.configPath);
        std::vector<deimos::Diagnostic> diagnostics;
        const deimos::Status status = store.load(&diagnostics);
        for (const deimos::Diagnostic& diagnostic : diagnostics)
        {
            std::cout << deimos::toString(diagnostic.severity) << ": " << diagnostic.message;
            if (!diagnostic.path.empty())
            {
                std::cout << "  [" << diagnostic.path << "]";
            }
            std::cout << "\n";
        }
        if (!status.isOk())
        {
            std::cerr << "deimosd: " << status.error().describe() << "\n";
            return 1;
        }
        std::cout << "configuration is valid\n";
        return 0;
    }

    if (commandLine.installService || commandLine.uninstallService)
    {
        std::string message;
        const deimos::Status status = commandLine.installService
                                          ? deimos::service::install(installOptionsFrom(commandLine), message)
                                          : deimos::service::uninstall(commandLine.serviceName, message);
        if (!message.empty())
        {
            std::cout << message << "\n";
        }
        if (!status.isOk())
        {
            std::cerr << "deimosd: " << status.error().describe() << "\n";
            return 1;
        }
        return 0;
    }

#ifdef _WIN32
    if (deimos::service::isRunningAsService())
    {
        deimos::DaemonApp serviceApp(commandLine.daemon);
        const auto serviceRun = [&serviceApp]
        {
            if (!serviceApp.initialize())
            {
                return 1;
            }
            if (!serviceApp.start())
            {
                return 1;
            }
            serviceApp.waitForStop([] { return false; });
            serviceApp.shutdown();
            return 0;
        };
        const auto requestStop = [&serviceApp]
        {
            serviceApp.requestStop();
        };

        std::string serviceError;
        if (deimos::service::runAsService(commandLine.serviceName, serviceRun, requestStop, serviceError))
        {
            return 0;
        }
        std::cerr << "deimosd: " << serviceError << "\n";
        return 1;
    }
#endif

    return runDaemon(commandLine, false);
}
