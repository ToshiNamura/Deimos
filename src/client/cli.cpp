#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <deimos/model/config.hpp>
#include <deimos/model/profile.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>
#include <deimos/transport/daemon_client.hpp>
#include <deimos/version.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace deimos
{

namespace
{

std::atomic<bool> g_interrupted{false};

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD eventType)
{
    if (eventType == CTRL_C_EVENT || eventType == CTRL_BREAK_EVENT)
    {
        g_interrupted.store(true);
        return TRUE;
    }
    return FALSE;
}
#else
extern "C" void signalHandler(int)
{
    g_interrupted.store(true);
}
#endif

constexpr int EXIT_OK = 0;
constexpr int EXIT_FAILURE_CODE = 1;
constexpr int EXIT_USAGE = 2;
constexpr int EXIT_CONNECTION = 3;
constexpr int EXIT_UNAUTHORIZED = 4;

struct CliArguments
{
    std::vector<std::string> positionals;
    std::map<std::string, std::string> values;
    std::set<std::string> flags;

    bool has(std::string_view name) const
    {
        return flags.count(std::string{name}) > 0 || values.count(std::string{name}) > 0;
    }

    std::string value(std::string_view name, std::string fallback = {}) const
    {
        const auto iterator = values.find(std::string{name});
        return iterator == values.end() ? fallback : iterator->second;
    }
};

CliArguments parseArguments(int argc, char** argv)
{
    static const std::set<std::string> VALUE_OPTIONS{
        "endpoint", "token", "token-file", "profile", "limit", "level", "output", "name", "url", "timeout", "config"};

    CliArguments parsed;
    for (int index = 1; index < argc; ++index)
    {
        std::string argument = argv[index];
        if (argument.size() > 2 && argument.rfind("--", 0) == 0)
        {
            const std::string name = argument.substr(2);
            if (const std::size_t equals = name.find('='); equals != std::string::npos)
            {
                parsed.values[name.substr(0, equals)] = name.substr(equals + 1);
                continue;
            }
            if (VALUE_OPTIONS.count(name) > 0 && index + 1 < argc)
            {
                parsed.values[name] = argv[++index];
                continue;
            }
            parsed.flags.insert(name);
            continue;
        }
        if (argument == "-h")
        {
            parsed.flags.insert("help");
            continue;
        }
        if (argument == "-v")
        {
            parsed.flags.insert("version");
            continue;
        }
        parsed.positionals.push_back(argument);
    }
    return parsed;
}

struct EndpointConfig
{
    std::string url;
    std::string tokenFile;
};

struct CliState
{
    std::string current;
    std::map<std::string, EndpointConfig> endpoints;
};

std::filesystem::path cliStatePath()
{
    return deimos::platform::configDirectory() / "cli.json";
}

CliState loadCliState()
{
    CliState state;
    const auto content = deimos::platform::readFile(cliStatePath());
    if (!content.has_value())
    {
        return state;
    }

    try
    {
        const auto root = nlohmann::json::parse(*content);
        state.current = root.value("current", std::string{});
        const auto endpoints = root.find("endpoints");
        if (endpoints != root.end() && endpoints->is_object())
        {
            for (auto iterator = endpoints->begin(); iterator != endpoints->end(); ++iterator)
            {
                EndpointConfig endpoint;
                endpoint.url = iterator->value("url", std::string{});
                endpoint.tokenFile = iterator->value("tokenFile", std::string{});
                state.endpoints[iterator.key()] = endpoint;
            }
        }
    }
    catch (const std::exception&)
    {
    }
    return state;
}

bool saveCliState(const CliState& state)
{
    nlohmann::json endpoints = nlohmann::json::object();
    for (const auto& [name, endpoint] : state.endpoints)
    {
        nlohmann::json entry;
        entry["url"] = endpoint.url;
        if (!endpoint.tokenFile.empty())
        {
            entry["tokenFile"] = endpoint.tokenFile;
        }
        endpoints[name] = entry;
    }

    nlohmann::json root;
    root["current"] = state.current;
    root["endpoints"] = endpoints;

    deimos::platform::ensureDirectory(cliStatePath().parent_path());
    std::string errorMessage;
    if (!deimos::platform::writeFileAtomically(cliStatePath(), root.dump(2) + "\n", errorMessage))
    {
        std::cerr << "deimos: cannot write " << cliStatePath().string() << ": " << errorMessage << "\n";
        return false;
    }
    return true;
}

std::string readTokenFile(const std::string& path)
{
    const auto content = deimos::platform::readFile(std::filesystem::path{path});
    if (!content.has_value())
    {
        return {};
    }
    return deimos::strings::trim(*content);
}

struct GlobalOptions
{
    std::string endpoint;
    std::string token;
    bool json = false;
    int timeoutSeconds = 10;
};

GlobalOptions resolveGlobals(const CliArguments& arguments)
{
    GlobalOptions options;
    options.json = arguments.has("json");

    const CliState state = loadCliState();

    options.endpoint = arguments.value("endpoint");
    if (options.endpoint.empty())
    {
        if (const auto fromEnvironment = deimos::platform::environmentVariable("DEIMOS_ENDPOINT"))
        {
            options.endpoint = *fromEnvironment;
        }
    }
    if (options.endpoint.empty() && !state.current.empty())
    {
        if (const auto iterator = state.endpoints.find(state.current); iterator != state.endpoints.end())
        {
            options.endpoint = iterator->second.url;
        }
    }
    if (options.endpoint.empty())
    {
        options.endpoint = "http://127.0.0.1:8787";
    }

    options.token = arguments.value("token");
    if (options.token.empty())
    {
        if (const auto fromEnvironment = deimos::platform::environmentVariable("DEIMOS_TOKEN"))
        {
            options.token = *fromEnvironment;
        }
    }
    if (options.token.empty())
    {
        if (const std::string tokenFile = arguments.value("token-file"); !tokenFile.empty())
        {
            options.token = readTokenFile(tokenFile);
        }
    }
    if (options.token.empty() && !state.current.empty())
    {
        if (const auto iterator = state.endpoints.find(state.current);
            iterator != state.endpoints.end() && !iterator->second.tokenFile.empty())
        {
            options.token = readTokenFile(iterator->second.tokenFile);
        }
    }
    if (options.token.empty())
    {
        options.token = readTokenFile(deimos::defaultTokenPath());
    }

    if (const std::string timeout = arguments.value("timeout"); !timeout.empty())
    {
        std::int64_t parsed = 0;
        if (deimos::strings::parseInt(timeout, parsed) && parsed > 0 && parsed <= 600)
        {
            options.timeoutSeconds = static_cast<int>(parsed);
        }
    }
    return options;
}

int exitCodeFor(const deimos::Error& error)
{
    switch (error.code())
    {
    case deimos::ErrorCode::network:
        return EXIT_CONNECTION;
    case deimos::ErrorCode::unauthorized:
        return EXIT_UNAUTHORIZED;
    default:
        return EXIT_FAILURE_CODE;
    }
}

deimos::DaemonClient makeClient(const GlobalOptions& options)
{
    deimos::Endpoint endpoint;
    std::string errorMessage;
    if (!deimos::parseEndpoint(options.endpoint, endpoint, errorMessage))
    {
        std::cerr << "deimos: invalid endpoint '" << options.endpoint << "': " << errorMessage << "\n";
    }

    deimos::ClientOptions clientOptions;
    clientOptions.endpoint = endpoint;
    clientOptions.token = options.token;
    clientOptions.timeoutSeconds = options.timeoutSeconds;
    return deimos::DaemonClient{clientOptions};
}

std::string formatDuration(std::int64_t seconds)
{
    if (seconds < 0)
    {
        return "-";
    }
    const std::int64_t hours = seconds / 3600;
    const std::int64_t minutes = (seconds % 3600) / 60;
    const std::int64_t remainder = seconds % 60;

    std::ostringstream stream;
    if (hours > 0)
    {
        stream << hours << "h ";
    }
    if (hours > 0 || minutes > 0)
    {
        stream << minutes << "m ";
    }
    stream << remainder << "s";
    return stream.str();
}

std::string padRight(const std::string& text, std::size_t width)
{
    if (text.size() >= width)
    {
        return text;
    }
    return text + std::string(width - text.size(), ' ');
}

std::string yesNo(bool value)
{
    return value ? "yes" : "no";
}

int fail(const deimos::Error& error)
{
    std::cerr << "deimos: " << error.describe() << "\n";
    return exitCodeFor(error);
}

void printUsage()
{
    std::cout << R"(deimos - Deimos client: desktop interface and command line

Usage: deimos [global options] <command> [arguments]
       deimos --gui            open the graphical interface
       deimos                  opens the graphical interface when started without a terminal

Global options:
  --endpoint URL      Daemon endpoint (default: $DEIMOS_ENDPOINT or http://127.0.0.1:8787)
  --token TOKEN       Bearer token (default: $DEIMOS_TOKEN or the token file)
  --token-file PATH   Read the bearer token from a file
  --timeout SECONDS   Request timeout, default 10
  --json              Print raw JSON instead of tables
  --gui               Start the desktop interface instead of running a command
  -h, --help          Show this help
  -v, --version       Show the client version

Commands:
  status                      Daemon, zapret and engine state
  start [--profile ID]        Start the engine with the active or given profile
  stop                        Stop the engine
  restart [--profile ID]      Restart the engine
  profiles list               List profiles
  profiles show ID            Print one profile as JSON
  profiles preview ID         Show the zapret command lines for a profile
  profiles activate ID        Make a profile active (restarts the engine when running)
  profiles import FILE        Create or replace a profile from a JSON file
  profiles export ID          Print a profile as JSON (--output FILE to write it)
  profiles delete ID          Delete a profile
  logs [--limit N]            Recent log entries
       [--level L] [--follow]
  config show                 Print the daemon configuration
  config path                 Print the local configuration path
  doctor                      Check connectivity, authentication and zapret
  endpoints list              List saved endpoints
  endpoints add NAME URL [--token-file PATH]
  endpoints use NAME          Select the default endpoint
  endpoints remove NAME       Delete a saved endpoint

Exit codes: 0 ok, 1 error, 2 usage, 3 connection, 4 authentication
)";
}

int commandStatus(const CliArguments& arguments, const GlobalOptions& options)
{
    auto client = makeClient(options);

    if (options.json)
    {
        const auto raw = client.call("GET", "/api/v1/status");
        if (!raw)
        {
            return fail(raw.error());
        }
        std::cout << raw.value() << "\n";
        return EXIT_OK;
    }

    const auto status = client.status();
    if (!status)
    {
        return fail(status.error());
    }

    const deimos::DaemonInfo& info = status.value();
    std::cout << "Endpoint   " << client.endpoint().describe() << "\n";
    std::cout << "Daemon     " << info.version << " on " << info.platform << "/" << info.architecture << " ("
              << info.hostname << ")\n";
    std::cout << "Uptime     " << formatDuration(info.uptimeSeconds) << "\n";
    std::cout << "Auth       " << (info.authenticationRequired ? "bearer token required" : "disabled") << "\n";
    std::cout << "zapret     "
              << (info.zapretAvailable ? deimos::strings::join(info.zapretTools, ", ") : std::string{"not detected"})
              << "\n";
    if (!info.zapretBaseDirectory.empty())
    {
        std::cout << "           " << info.zapretBaseDirectory << "\n";
    }

    const deimos::EngineSnapshot& engine = info.engine;
    std::cout << "Engine     " << deimos::toString(engine.state);
    if (!engine.activeProfileId.empty())
    {
        std::cout << " (profile " << engine.activeProfileId << ")";
    }
    std::cout << "\n";

    if (engine.state == deimos::EngineState::running)
    {
        std::cout << "Running    " << formatDuration(engine.uptimeSeconds) << ", restarts " << engine.restartCount
                  << "\n";
    }
    if (!engine.lastError.empty())
    {
        std::cout << "Last error " << engine.lastError << "\n";
    }
    for (const deimos::LaunchInfo& launch : engine.launches)
    {
        std::cout << "Process    " << launch.label << " (pid " << launch.pid << ")\n";
        std::cout << "           " << launch.command << "\n";
    }

    (void)arguments;
    return EXIT_OK;
}

int printEngineResult(const deimos::Result<deimos::EngineSnapshot>& result, const GlobalOptions& options)
{
    if (!result)
    {
        return fail(result.error());
    }
    if (options.json)
    {
        std::cout << deimos::serializeEngineSnapshot(result.value(), true) << "\n";
        return EXIT_OK;
    }

    const deimos::EngineSnapshot& snapshot = result.value();
    std::cout << "engine " << deimos::toString(snapshot.state);
    if (!snapshot.activeProfileId.empty())
    {
        std::cout << " with profile " << snapshot.activeProfileId;
    }
    std::cout << "\n";
    for (const deimos::LaunchInfo& launch : snapshot.launches)
    {
        std::cout << "  " << launch.label << " (pid " << launch.pid << ")\n";
    }
    for (const deimos::Diagnostic& diagnostic : snapshot.diagnostics)
    {
        std::cout << "  " << deimos::toString(diagnostic.severity) << ": " << diagnostic.message << "\n";
    }
    return EXIT_OK;
}

int commandProfiles(const CliArguments& arguments, const GlobalOptions& options)
{
    const std::string subcommand = arguments.positionals.size() > 1 ? arguments.positionals[1] : "list";
    auto client = makeClient(options);

    if (subcommand == "list")
    {
        if (options.json)
        {
            const auto raw = client.call("GET", "/api/v1/profiles");
            if (!raw)
            {
                return fail(raw.error());
            }
            std::cout << raw.value() << "\n";
            return EXIT_OK;
        }

        const auto profiles = client.listProfiles();
        if (!profiles)
        {
            return fail(profiles.error());
        }
        if (profiles.value().empty())
        {
            std::cout << "no profiles\n";
            return EXIT_OK;
        }

        std::cout << padRight("ID", 24) << padRight("NAME", 28) << padRight("TOOL", 8) << padRight("ROUNDS", 8)
                  << "ENABLED\n";
        for (const deimos::ProfileSummary& summary : profiles.value())
        {
            std::cout << padRight(summary.id, 24) << padRight(summary.name, 28)
                      << padRight(std::string{deimos::toCliToken(summary.tool)}, 8)
                      << padRight(std::to_string(summary.roundCount), 8) << yesNo(summary.enabled) << "\n";
        }
        return EXIT_OK;
    }

    const std::string id = arguments.positionals.size() > 2 ? arguments.positionals[2] : std::string{};
    if (id.empty() && subcommand != "import")
    {
        std::cerr << "deimos: profiles " << subcommand << " needs a profile id\n";
        return EXIT_USAGE;
    }

    if (subcommand == "show")
    {
        const auto profile = client.getProfile(id);
        if (!profile)
        {
            return fail(profile.error());
        }
        std::cout << deimos::serializeProfile(profile.value(), true) << "\n";
        return EXIT_OK;
    }

    if (subcommand == "preview")
    {
        if (options.json)
        {
            const auto raw = client.call("GET", "/api/v1/profiles/" + id + "/preview");
            if (!raw)
            {
                return fail(raw.error());
            }
            std::cout << raw.value() << "\n";
            return EXIT_OK;
        }

        const auto preview = client.previewProfile(id);
        if (!preview)
        {
            return fail(preview.error());
        }
        for (const std::string& command : preview.value().commands)
        {
            std::cout << command << "\n";
        }
        for (const deimos::Diagnostic& diagnostic : preview.value().diagnostics)
        {
            std::cout << "  " << deimos::toString(diagnostic.severity) << ": " << diagnostic.message << "\n";
        }
        return EXIT_OK;
    }

    if (subcommand == "activate")
    {
        const auto result = client.activateProfile(id);
        return printEngineResult(result, options);
    }

    if (subcommand == "delete" || subcommand == "remove")
    {
        const auto status = client.deleteProfile(id);
        if (!status.isOk())
        {
            return fail(status.error());
        }
        std::cout << "profile " << id << " deleted\n";
        return EXIT_OK;
    }

    if (subcommand == "export")
    {
        const auto profile = client.getProfile(id);
        if (!profile)
        {
            return fail(profile.error());
        }
        const std::string text = deimos::serializeProfile(profile.value(), true) + "\n";
        const std::string output = arguments.value("output");
        if (output.empty())
        {
            std::cout << text;
            return EXIT_OK;
        }
        std::string errorMessage;
        if (!deimos::platform::writeFile(output, text, errorMessage))
        {
            std::cerr << "deimos: " << errorMessage << "\n";
            return EXIT_FAILURE_CODE;
        }
        std::cout << "written to " << output << "\n";
        return EXIT_OK;
    }

    if (subcommand == "import")
    {
        const std::string file = arguments.positionals.size() > 2 ? arguments.positionals[2] : std::string{};
        if (file.empty())
        {
            std::cerr << "deimos: profiles import needs a file\n";
            return EXIT_USAGE;
        }
        const auto content = deimos::platform::readFile(file);
        if (!content.has_value())
        {
            std::cerr << "deimos: cannot read " << file << "\n";
            return EXIT_FAILURE_CODE;
        }

        deimos::Profile profile;
        std::vector<deimos::Diagnostic> diagnostics;
        if (const auto status = deimos::parseProfile(*content, profile, diagnostics); !status.isOk())
        {
            std::cerr << "deimos: " << status.error().describe() << "\n";
            std::cerr << deimos::formatDiagnostics(diagnostics);
            return EXIT_FAILURE_CODE;
        }

        const auto saved = client.saveProfile(profile);
        if (!saved)
        {
            return fail(saved.error());
        }
        std::cout << "profile " << saved.value().id << " saved\n";
        return EXIT_OK;
    }

    std::cerr << "deimos: unknown profiles subcommand '" << subcommand << "'\n";
    return EXIT_USAGE;
}

int commandLogs(const CliArguments& arguments, const GlobalOptions& options)
{
    auto client = makeClient(options);

    std::size_t limit = 200;
    if (const std::string text = arguments.value("limit"); !text.empty())
    {
        std::int64_t parsed = 0;
        if (deimos::strings::parseInt(text, parsed) && parsed > 0)
        {
            limit = static_cast<std::size_t>(parsed);
        }
    }

    deimos::LogLevel level = deimos::LogLevel::trace;
    if (const std::string text = arguments.value("level"); !text.empty())
    {
        if (!deimos::parseLogLevel(text, level))
        {
            std::cerr << "deimos: unknown log level '" << text << "'\n";
            return EXIT_USAGE;
        }
    }

    std::uint64_t lastSequence = 0;
    const auto entries = client.logs(limit, level);
    if (!entries)
    {
        return fail(entries.error());
    }
    for (const deimos::LogEntry& entry : entries.value())
    {
        std::cout << deimos::formatTimestamp(entry.timestamp) << " " << padRight(std::string{deimos::toString(entry.level)}, 6)
                  << " [" << entry.source << "] " << entry.message << "\n";
        lastSequence = entry.sequence;
    }

    if (!arguments.has("follow"))
    {
        return EXIT_OK;
    }

    while (!g_interrupted.load())
    {
#ifdef _WIN32
        Sleep(1000);
#else
        ::sleep(1);
#endif
        const auto followEntries = client.logs(limit, level, lastSequence);
        if (!followEntries)
        {
            return fail(followEntries.error());
        }
        for (const deimos::LogEntry& entry : followEntries.value())
        {
            std::cout << deimos::formatTimestamp(entry.timestamp) << " "
                      << padRight(std::string{deimos::toString(entry.level)}, 6) << " [" << entry.source << "] "
                      << entry.message << "\n";
            lastSequence = entry.sequence;
        }
    }
    return EXIT_OK;
}

int commandConfig(const CliArguments& arguments, const GlobalOptions& options)
{
    const std::string subcommand = arguments.positionals.size() > 1 ? arguments.positionals[1] : "show";

    if (subcommand == "path")
    {
        std::cout << deimos::defaultConfigPath() << "\n";
        return EXIT_OK;
    }

    if (subcommand != "show")
    {
        std::cerr << "deimos: unknown config subcommand '" << subcommand << "'\n";
        return EXIT_USAGE;
    }

    auto client = makeClient(options);
    if (options.json)
    {
        const auto raw = client.call("GET", "/api/v1/config");
        if (!raw)
        {
            return fail(raw.error());
        }
        std::cout << raw.value() << "\n";
        return EXIT_OK;
    }

    const auto config = client.getConfig();
    if (!config)
    {
        return fail(config.error());
    }
    std::cout << deimos::serializeConfig(config.value(), true) << "\n";
    return EXIT_OK;
}

int commandDoctor(const CliArguments& arguments, const GlobalOptions& options)
{
    auto client = makeClient(options);
    bool healthy = true;

    std::cout << "endpoint      " << client.endpoint().describe() << "\n";

    const auto health = client.health();
    if (!health)
    {
        std::cout << "reachability  FAILED: " << health.error().describe() << "\n";
        std::cout << "\nhint: start the daemon with 'deimosd --foreground'\n";
        return EXIT_CONNECTION;
    }
    std::cout << "reachability  ok (" << health.value().version << ")\n";

    if (client.hasToken())
    {
        const auto status = client.status();
        if (!status)
        {
            healthy = false;
            std::cout << "authentication FAILED: " << status.error().describe() << "\n";
            std::cout << "\nhint: check $DEIMOS_TOKEN or the token file written by 'deimosd --generate-token'\n";
        }
        else
        {
            std::cout << "authentication ok\n";
            std::cout << "daemon        " << status.value().platform << "/" << status.value().architecture << ", up "
                      << formatDuration(status.value().uptimeSeconds) << "\n";
        }
    }
    else
    {
        std::cout << "authentication no token configured (fine when the daemon disables auth)\n";
    }

    const auto zapret = client.zapret();
    if (!zapret)
    {
        healthy = false;
        std::cout << "zapret        FAILED: " << zapret.error().describe() << "\n";
    }
    else if (!zapret.value().available)
    {
        healthy = false;
        std::cout << "zapret        not detected\n";
        for (const deimos::Diagnostic& diagnostic : zapret.value().diagnostics)
        {
            std::cout << "              " << diagnostic.message << "\n";
        }
        std::cout << "\nhint: set zapret.installDirectory in the daemon configuration\n";
    }
    else
    {
        std::cout << "zapret        " << deimos::strings::join(zapret.value().tools, ", ") << "\n";
        if (!zapret.value().baseDirectory.empty())
        {
            std::cout << "              " << zapret.value().baseDirectory << "\n";
        }
    }

    (void)arguments;
    return healthy ? EXIT_OK : EXIT_FAILURE_CODE;
}

int commandEndpoints(const CliArguments& arguments, const GlobalOptions& options)
{
    (void)options;
    const std::string subcommand = arguments.positionals.size() > 1 ? arguments.positionals[1] : "list";
    CliState state = loadCliState();

    if (subcommand == "list")
    {
        if (state.endpoints.empty())
        {
            std::cout << "no saved endpoints (pass --endpoint to talk to a daemon)\n";
            return EXIT_OK;
        }
        for (const auto& [name, endpoint] : state.endpoints)
        {
            std::cout << (name == state.current ? "* " : "  ") << padRight(name, 16) << endpoint.url << "\n";
        }
        return EXIT_OK;
    }

    if (subcommand == "add")
    {
        const std::string name = arguments.positionals.size() > 2 ? arguments.positionals[2] : std::string{};
        const std::string url = arguments.positionals.size() > 3 ? arguments.positionals[3]
                                : arguments.value("url").empty()       ? std::string{}
                                                                       : arguments.value("url");
        if (name.empty() || url.empty())
        {
            std::cerr << "deimos: endpoints add NAME URL\n";
            return EXIT_USAGE;
        }

        deimos::Endpoint parsed;
        std::string errorMessage;
        if (!deimos::parseEndpoint(url, parsed, errorMessage))
        {
            std::cerr << "deimos: " << errorMessage << "\n";
            return EXIT_USAGE;
        }

        EndpointConfig endpoint;
        endpoint.url = parsed.describe();
        endpoint.tokenFile = arguments.value("token-file");
        state.endpoints[name] = endpoint;
        if (state.current.empty())
        {
            state.current = name;
        }
        if (!saveCliState(state))
        {
            return EXIT_FAILURE_CODE;
        }
        std::cout << "endpoint " << name << " saved\n";
        return EXIT_OK;
    }

    if (subcommand == "use")
    {
        const std::string name = arguments.positionals.size() > 2 ? arguments.positionals[2] : std::string{};
        if (state.endpoints.count(name) == 0)
        {
            std::cerr << "deimos: unknown endpoint '" << name << "'\n";
            return EXIT_FAILURE_CODE;
        }
        state.current = name;
        if (!saveCliState(state))
        {
            return EXIT_FAILURE_CODE;
        }
        std::cout << "current endpoint: " << name << "\n";
        return EXIT_OK;
    }

    if (subcommand == "remove")
    {
        const std::string name = arguments.positionals.size() > 2 ? arguments.positionals[2] : std::string{};
        if (state.endpoints.erase(name) == 0)
        {
            std::cerr << "deimos: unknown endpoint '" << name << "'\n";
            return EXIT_FAILURE_CODE;
        }
        if (state.current == name)
        {
            state.current.clear();
        }
        if (!saveCliState(state))
        {
            return EXIT_FAILURE_CODE;
        }
        std::cout << "endpoint " << name << " removed\n";
        return EXIT_OK;
    }

    std::cerr << "deimos: unknown endpoints subcommand '" << subcommand << "'\n";
    return EXIT_USAGE;
}

} // namespace

int runCli(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN);
#endif

    const CliArguments arguments = parseArguments(argc, argv);

    if (arguments.has("version"))
    {
        std::cout << "deimos " << deimos::versionString() << "\n";
        return EXIT_OK;
    }

    if (arguments.has("help") || arguments.positionals.empty())
    {
        printUsage();
        return arguments.has("help") ? EXIT_OK : EXIT_USAGE;
    }

    const GlobalOptions options = resolveGlobals(arguments);
    const std::string command = arguments.positionals[0];

    if (command == "version")
    {
        std::cout << "deimos " << deimos::versionString() << "\n";
        return EXIT_OK;
    }
    if (command == "status")
    {
        return commandStatus(arguments, options);
    }
    if (command == "start")
    {
        auto client = makeClient(options);
        const std::string profileId = arguments.value("profile");
        return printEngineResult(client.startEngine(profileId), options);
    }
    if (command == "stop")
    {
        auto client = makeClient(options);
        return printEngineResult(client.stopEngine(), options);
    }
    if (command == "restart")
    {
        auto client = makeClient(options);
        const std::string profileId = arguments.value("profile");
        return printEngineResult(client.restartEngine(profileId), options);
    }
    if (command == "profiles")
    {
        return commandProfiles(arguments, options);
    }
    if (command == "logs")
    {
        return commandLogs(arguments, options);
    }
    if (command == "config")
    {
        return commandConfig(arguments, options);
    }
    if (command == "doctor")
    {
        return commandDoctor(arguments, options);
    }
    if (command == "endpoints")
    {
        return commandEndpoints(arguments, options);
    }

    std::cerr << "deimos: unknown command '" << command << "'\n\n";
    printUsage();
    return EXIT_USAGE;
}

} // namespace deimos
