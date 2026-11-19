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
#include <deimos/support/i18n.hpp>
#include <deimos/support/labels.hpp>
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
    struct OptionHelp
    {
        const char* flag;
        const char* key;
    };

    static const OptionHelp OPTIONS[] = {
        {"  --config PATH            ", "daemon.usage.config"},
        {"  --listen ADDRESS         ", "daemon.usage.listen"},
        {"  --port PORT              ", "daemon.usage.port"},
        {"  --log-level LEVEL        ", "daemon.usage.logLevel"},
        {"  --lang LANG              ", "daemon.usage.lang"},
        {"  --foreground             ", "daemon.usage.foreground"},
        {"  --service-run            ", "daemon.usage.serviceRun"},
        {"  --service-name NAME      ", "daemon.usage.serviceName"},
        {"  --install-service        ", "daemon.usage.installService"},
        {"  --uninstall-service      ", "daemon.usage.uninstallService"},
        {"  --print-unit             ", "daemon.usage.printUnit"},
        {"  --print-plist            ", "daemon.usage.printPlist"},
        {"  --generate-token         ", "daemon.usage.generateToken"},
        {"  --token-file PATH        ", "daemon.usage.tokenFile"},
        {"  --validate-config        ", "daemon.usage.validateConfig"},
        {"  --version                ", "daemon.usage.version"},
        {"  --help                   ", "daemon.usage.help"},
    };

    std::cout << deimos::i18n::tr("daemon.usage.title") << "\n\n"
              << deimos::i18n::tr("daemon.usage.synopsis") << "\n\n"
              << deimos::i18n::tr("daemon.usage.options") << "\n";
    for (const OptionHelp& option : OPTIONS)
    {
        std::cout << option.flag << deimos::i18n::tr(option.key) << "\n";
    }
    std::cout << std::flush;
}

void applyLanguageFromArguments(int argc, char** argv)
{
    deimos::i18n::setLanguage(deimos::i18n::Language::system);
    const std::string prefix{"--lang="};
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--lang" && index + 1 < argc)
        {
            deimos::i18n::setLanguageByCode(argv[index + 1]);
            return;
        }
        if (argument.compare(0, prefix.size(), prefix) == 0)
        {
            deimos::i18n::setLanguageByCode(argument.substr(prefix.size()));
            return;
        }
    }
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
            std::cerr << deimos::i18n::tr("daemon.usage.missingValue", {argument}) << "\n";
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
                    std::cerr << deimos::i18n::tr("daemon.usage.invalidPort", {text}) << "\n";
                    parsed.valid = false;
                }
            }
        }
        else if (argument == "--log-level")
        {
            value(parsed.daemon.logLevelOverride);
        }
        else if (argument == "--lang")
        {
            value(parsed.daemon.languageOverride);
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
            std::cerr << deimos::i18n::tr("daemon.usage.unknownOption", {argument}) << "\n";
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
        std::cout << deimos::i18n::tr("daemon.running") << "\n";
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

    applyLanguageFromArguments(argc, argv);

    const CommandLine commandLine = parseCommandLine(argc, argv);
    if (!commandLine.valid)
    {
        printUsage();
        return 2;
    }

    if (!commandLine.daemon.languageOverride.empty())
    {
        deimos::i18n::setLanguageByCode(commandLine.daemon.languageOverride);
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
        std::cerr << deimos::i18n::tr("daemon.tokenWritten", {store.tokenFile()}) << "\n";
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
            std::cout << deimos::labels::severity(diagnostic.severity) << ": " << diagnostic.message;
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
        std::cout << deimos::i18n::tr("daemon.configValid") << "\n";
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
