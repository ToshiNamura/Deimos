#include <deimos/transport/protocol.hpp>

#include <exception>

#include <nlohmann/json.hpp>

#include <deimos/support/i18n.hpp>
#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

using Json = nlohmann::json;

Json diagnosticsToJson(const std::vector<Diagnostic>& diagnostics)
{
    Json array = Json::array();
    for (const Diagnostic& diagnostic : diagnostics)
    {
        array.push_back(Json{
            {"severity", std::string{toString(diagnostic.severity)}},
            {"code", diagnostic.code},
            {"message", diagnostic.message},
            {"path", diagnostic.path},
        });
    }
    return array;
}

void readDiagnostics(const Json& node, std::vector<Diagnostic>& out)
{
    if (!node.is_array())
    {
        return;
    }
    for (const Json& item : node)
    {
        if (!item.is_object())
        {
            continue;
        }
        Diagnostic diagnostic;
        const std::string severity = item.value("severity", std::string{"info"});
        diagnostic.severity = severity == "error"     ? Severity::error
                              : severity == "warning" ? Severity::warning
                                                      : Severity::info;
        diagnostic.code = item.value("code", std::string{});
        diagnostic.message = item.value("message", std::string{});
        diagnostic.path = item.value("path", std::string{});
        out.push_back(std::move(diagnostic));
    }
}

Json engineToJson(const EngineSnapshot& snapshot)
{
    Json launches = Json::array();
    for (const LaunchInfo& launch : snapshot.launches)
    {
        launches.push_back(Json{
            {"label", launch.label},
            {"command", launch.command},
            {"pid", launch.pid},
        });
    }

    Json node{
        {"state", std::string{toString(snapshot.state)}},
        {"activeProfileId", snapshot.activeProfileId},
        {"uptimeSeconds", snapshot.uptimeSeconds},
        {"restartCount", snapshot.restartCount},
        {"launches", std::move(launches)},
    };
    if (!snapshot.lastError.empty())
    {
        node["lastError"] = snapshot.lastError;
    }
    if (!snapshot.diagnostics.empty())
    {
        node["diagnostics"] = diagnosticsToJson(snapshot.diagnostics);
    }
    return node;
}

EngineSnapshot engineFromJson(const Json& node)
{
    EngineSnapshot snapshot;
    parseEngineState(node.value("state", std::string{"stopped"}), snapshot.state);
    snapshot.activeProfileId = node.value("activeProfileId", std::string{});
    snapshot.uptimeSeconds = node.value("uptimeSeconds", static_cast<std::int64_t>(0));
    snapshot.restartCount = node.value("restartCount", 0);
    snapshot.lastError = node.value("lastError", std::string{});

    if (const auto launches = node.find("launches"); launches != node.end() && launches->is_array())
    {
        for (const Json& item : *launches)
        {
            if (!item.is_object())
            {
                continue;
            }
            LaunchInfo info;
            info.label = item.value("label", std::string{});
            info.command = item.value("command", std::string{});
            info.pid = item.value("pid", static_cast<std::uint64_t>(0));
            snapshot.launches.push_back(std::move(info));
        }
    }
    if (const auto diagnostics = node.find("diagnostics"); diagnostics != node.end())
    {
        readDiagnostics(*diagnostics, snapshot.diagnostics);
    }
    return snapshot;
}

Json logEntryToJson(const LogEntry& entry)
{
    return Json{
        {"sequence", entry.sequence},
        {"time", formatTimestamp(entry.timestamp)},
        {"level", std::string{toString(entry.level)}},
        {"source", entry.source},
        {"message", entry.message},
    };
}

template <typename ParseFn>
Status withJson(std::string_view text, ParseFn&& parse)
{
    try
    {
        const Json root = Json::parse(text);
        if (!root.is_object())
        {
            return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.protocol.notObject"));
        }
        parse(root);
        return Status::ok();
    }
    catch (const std::exception& exception)
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.protocol.parse"), exception.what());
    }
}

} // namespace

std::string_view toString(EngineState state)
{
    switch (state)
    {
    case EngineState::stopped:
        return "stopped";
    case EngineState::starting:
        return "starting";
    case EngineState::running:
        return "running";
    case EngineState::stopping:
        return "stopping";
    case EngineState::faulted:
        return "faulted";
    }
    return "stopped";
}

bool parseEngineState(std::string_view text, EngineState& out)
{
    const std::string value = strings::toLower(strings::trim(text));
    if (value == "stopped")
    {
        out = EngineState::stopped;
        return true;
    }
    if (value == "starting")
    {
        out = EngineState::starting;
        return true;
    }
    if (value == "running")
    {
        out = EngineState::running;
        return true;
    }
    if (value == "stopping")
    {
        out = EngineState::stopping;
        return true;
    }
    if (value == "faulted")
    {
        out = EngineState::faulted;
        return true;
    }
    return false;
}

std::string serializeHealth(const HealthInfo& info, bool pretty)
{
    const Json node{
        {"status", info.status},
        {"version", info.version},
        {"uptimeSeconds", info.uptimeSeconds},
    };
    return node.dump(pretty ? 2 : -1);
}

std::string serializeDaemonInfo(const DaemonInfo& info, bool pretty)
{
    Json tools = Json::array();
    for (const std::string& tool : info.zapretTools)
    {
        tools.push_back(tool);
    }

    const Json node{
        {"version", info.version},
        {"platform", info.platform},
        {"architecture", info.architecture},
        {"hostname", info.hostname},
        {"listenAddress", info.listenAddress},
        {"port", info.port},
        {"uptimeSeconds", info.uptimeSeconds},
        {"authenticationRequired", info.authenticationRequired},
        {"engine", engineToJson(info.engine)},
        {"zapret",
         Json{
             {"available", info.zapretAvailable},
             {"baseDirectory", info.zapretBaseDirectory},
             {"tools", std::move(tools)},
         }},
    };
    return node.dump(pretty ? 2 : -1);
}

std::string serializeEngineSnapshot(const EngineSnapshot& snapshot, bool pretty)
{
    return engineToJson(snapshot).dump(pretty ? 2 : -1);
}

std::string serializeCommandPreview(const CommandPreview& preview, bool pretty)
{
    Json commands = Json::array();
    for (const std::string& command : preview.commands)
    {
        commands.push_back(command);
    }
    const Json node{
        {"commands", std::move(commands)},
        {"diagnostics", diagnosticsToJson(preview.diagnostics)},
    };
    return node.dump(pretty ? 2 : -1);
}

std::string serializeLogEntries(const std::vector<LogEntry>& entries, std::uint64_t lastSequence, bool pretty)
{
    Json array = Json::array();
    for (const LogEntry& entry : entries)
    {
        array.push_back(logEntryToJson(entry));
    }
    const Json node{
        {"entries", std::move(array)},
        {"lastSequence", lastSequence},
    };
    return node.dump(pretty ? 2 : -1);
}

std::string serializeErrorPayload(const Error& error)
{
    const Json node{
        {"error",
         Json{
             {"code", std::string{toString(error.code())}},
             {"message", error.message()},
             {"detail", error.detail()},
         }},
    };
    return node.dump();
}

std::string serializeDiagnostics(const std::vector<Diagnostic>& diagnostics, bool pretty)
{
    return diagnosticsToJson(diagnostics).dump(pretty ? 2 : -1);
}

std::string serializeZapretInfo(const ZapretInfo& info, bool pretty)
{
    Json tools = Json::array();
    for (const std::string& tool : info.tools)
    {
        tools.push_back(tool);
    }

    const Json node{
        {"available", info.available},
        {"baseDirectory", info.baseDirectory},
        {"filesDirectory", info.filesDirectory},
        {"listsDirectory", info.listsDirectory},
        {"tools", std::move(tools)},
        {"diagnostics", diagnosticsToJson(info.diagnostics)},
    };
    return node.dump(pretty ? 2 : -1);
}

Status parseZapretInfo(std::string_view text, ZapretInfo& out)
{
    return withJson(text,
                    [&out](const Json& root)
                    {
                        ZapretInfo info;
                        info.available = root.value("available", false);
                        info.baseDirectory = root.value("baseDirectory", std::string{});
                        info.filesDirectory = root.value("filesDirectory", std::string{});
                        info.listsDirectory = root.value("listsDirectory", std::string{});
                        if (const auto tools = root.find("tools"); tools != root.end())
                        {
                            if (tools->is_array())
                            {
                                for (const Json& tool : *tools)
                                {
                                    if (tool.is_string())
                                    {
                                        info.tools.push_back(tool.get<std::string>());
                                    }
                                }
                            }
                            else if (tools->is_object())
                            {
                                for (auto iterator = tools->begin(); iterator != tools->end(); ++iterator)
                                {
                                    info.tools.push_back(iterator.key());
                                }
                            }
                        }
                        if (const auto diagnostics = root.find("diagnostics"); diagnostics != root.end())
                        {
                            readDiagnostics(*diagnostics, info.diagnostics);
                        }
                        out = std::move(info);
                    });
}

Status parseHealth(std::string_view text, HealthInfo& out)
{
    return withJson(text,
                    [&out](const Json& root)
                    {
                        out.status = root.value("status", std::string{"ok"});
                        out.version = root.value("version", std::string{});
                        out.uptimeSeconds = root.value("uptimeSeconds", static_cast<std::int64_t>(0));
                    });
}

Status parseDaemonInfo(std::string_view text, DaemonInfo& out)
{
    return withJson(text,
                    [&out](const Json& root)
                    {
                        DaemonInfo info;
                        info.version = root.value("version", std::string{});
                        info.platform = root.value("platform", std::string{});
                        info.architecture = root.value("architecture", std::string{});
                        info.hostname = root.value("hostname", std::string{});
                        info.listenAddress = root.value("listenAddress", std::string{});
                        const int port = root.value("port", 0);
                        info.port = port > 0 && port <= 65535 ? static_cast<std::uint16_t>(port) : 0;
                        info.uptimeSeconds = root.value("uptimeSeconds", static_cast<std::int64_t>(0));
                        info.authenticationRequired = root.value("authenticationRequired", true);

                        if (const auto engine = root.find("engine"); engine != root.end() && engine->is_object())
                        {
                            info.engine = engineFromJson(*engine);
                        }
                        if (const auto zapret = root.find("zapret"); zapret != root.end() && zapret->is_object())
                        {
                            info.zapretAvailable = zapret->value("available", false);
                            info.zapretBaseDirectory = zapret->value("baseDirectory", std::string{});
                            if (const auto tools = zapret->find("tools"); tools != zapret->end() && tools->is_array())
                            {
                                for (const Json& tool : *tools)
                                {
                                    if (tool.is_string())
                                    {
                                        info.zapretTools.push_back(tool.get<std::string>());
                                    }
                                }
                            }
                        }
                        out = std::move(info);
                    });
}

Status parseEngineSnapshot(std::string_view text, EngineSnapshot& out)
{
    return withJson(text,
                    [&out](const Json& root)
                    {
                        out = engineFromJson(root);
                    });
}

Status parseCommandPreview(std::string_view text, CommandPreview& out)
{
    return withJson(text,
                    [&out](const Json& root)
                    {
                        CommandPreview preview;
                        if (const auto commands = root.find("commands"); commands != root.end() && commands->is_array())
                        {
                            for (const Json& command : *commands)
                            {
                                if (command.is_string())
                                {
                                    preview.commands.push_back(command.get<std::string>());
                                }
                            }
                        }
                        if (const auto diagnostics = root.find("diagnostics"); diagnostics != root.end())
                        {
                            readDiagnostics(*diagnostics, preview.diagnostics);
                        }
                        out = std::move(preview);
                    });
}

Status parseLogEntries(std::string_view text, std::vector<LogEntry>& out, std::uint64_t& lastSequence)
{
    return withJson(text,
                    [&out, &lastSequence](const Json& root)
                    {
                        lastSequence = root.value("lastSequence", static_cast<std::uint64_t>(0));
                        const auto entries = root.find("entries");
                        if (entries == root.end() || !entries->is_array())
                        {
                            return;
                        }
                        for (const Json& item : *entries)
                        {
                            if (!item.is_object())
                            {
                                continue;
                            }
                            LogEntry entry;
                            entry.sequence = item.value("sequence", static_cast<std::uint64_t>(0));
                            const std::string timeText = item.value("time", std::string{});
                            if (!parseTimestamp(timeText, entry.timestamp))
                            {
                                entry.timestamp = std::chrono::system_clock::now();
                            }
                            const std::string levelText = item.value("level", std::string{"info"});
                            LogLevel level = LogLevel::info;
                            parseLogLevel(levelText, level);
                            entry.level = level;
                            entry.source = item.value("source", std::string{});
                            entry.message = item.value("message", std::string{});
                            out.push_back(std::move(entry));
                        }
                    });
}

Status parseDiagnostics(std::string_view text, std::vector<Diagnostic>& out)
{
    try
    {
        const Json root = Json::parse(text);
        if (root.is_array())
        {
            readDiagnostics(root, out);
            return Status::ok();
        }
        if (root.is_object())
        {
            if (const auto diagnostics = root.find("diagnostics"); diagnostics != root.end())
            {
                readDiagnostics(*diagnostics, out);
                return Status::ok();
            }
        }
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.protocol.noDiagnostics"));
    }
    catch (const std::exception& exception)
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.protocol.parseDiagnostics"), exception.what());
    }
}

Status parseErrorPayload(std::string_view text, Error& out)
{
    try
    {
        const Json root = Json::parse(text);
        const auto error = root.find("error");
        if (error == root.end() || !error->is_object())
        {
            return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.protocol.noErrorPayload"));
        }

        const std::string codeText = error->value("code", std::string{"internal_error"});
        ErrorCode code = ErrorCode::internal;
        for (const ErrorCode candidate : {ErrorCode::invalidArgument,
                                          ErrorCode::notFound,
                                          ErrorCode::alreadyExists,
                                          ErrorCode::notSupported,
                                          ErrorCode::configuration,
                                          ErrorCode::io,
                                          ErrorCode::network,
                                          ErrorCode::unauthorized,
                                          ErrorCode::conflict,
                                          ErrorCode::unavailable,
                                          ErrorCode::internal})
        {
            if (toString(candidate) == codeText)
            {
                code = candidate;
                break;
            }
        }

        out = Error{code, error->value("message", std::string{}), error->value("detail", std::string{})};
        return Status::ok();
    }
    catch (const std::exception& exception)
    {
        return Status::failure(ErrorCode::invalidArgument, i18n::tr("err.protocol.parseErrorPayload"), exception.what());
    }
}

}
