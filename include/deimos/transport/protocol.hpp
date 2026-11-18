#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/model/diagnostic.hpp>
#include <deimos/model/profile.hpp>
#include <deimos/support/error.hpp>
#include <deimos/support/log.hpp>

namespace deimos
{

/// Version prefix every endpoint lives under.
inline constexpr std::string_view API_PREFIX = "/api/v1";
inline constexpr std::string_view JSON_CONTENT_TYPE = "application/json";

enum class EngineState : std::uint8_t
{
    stopped = 0,
    starting,
    running,
    stopping,
    faulted,
};

std::string_view toString(EngineState state);
bool parseEngineState(std::string_view text, EngineState& out);

struct LaunchInfo
{
    std::string label;
    std::string command;
    std::uint64_t pid = 0;
};

struct EngineSnapshot
{
    EngineState state = EngineState::stopped;
    std::string activeProfileId;
    std::int64_t uptimeSeconds = 0;
    int restartCount = 0;
    std::vector<LaunchInfo> launches;
    std::string lastError;
    std::vector<Diagnostic> diagnostics;
};

struct DaemonInfo
{
    std::string version;
    std::string platform;
    std::string architecture;
    std::string hostname;
    std::string listenAddress;
    std::uint16_t port = 0;
    std::int64_t uptimeSeconds = 0;
    bool authenticationRequired = true;
    bool zapretAvailable = false;
    std::string zapretBaseDirectory;
    std::vector<std::string> zapretTools;
    EngineSnapshot engine;
};

struct HealthInfo
{
    std::string status{"ok"};
    std::string version;
    std::int64_t uptimeSeconds = 0;
};

struct CommandPreview
{
    std::vector<std::string> commands;
    std::vector<Diagnostic> diagnostics;
};

struct ZapretInfo
{
    bool available = false;
    std::string baseDirectory;
    std::string filesDirectory;
    std::string listsDirectory;
    std::vector<std::string> tools;
    std::vector<Diagnostic> diagnostics;
};

std::string serializeHealth(const HealthInfo& info, bool pretty = false);
std::string serializeDaemonInfo(const DaemonInfo& info, bool pretty = false);
std::string serializeEngineSnapshot(const EngineSnapshot& snapshot, bool pretty = false);
std::string serializeCommandPreview(const CommandPreview& preview, bool pretty = false);
std::string serializeLogEntries(const std::vector<LogEntry>& entries, std::uint64_t lastSequence, bool pretty = false);
std::string serializeErrorPayload(const Error& error);
std::string serializeDiagnostics(const std::vector<Diagnostic>& diagnostics, bool pretty = false);
std::string serializeZapretInfo(const ZapretInfo& info, bool pretty = false);

Status parseHealth(std::string_view text, HealthInfo& out);
Status parseDaemonInfo(std::string_view text, DaemonInfo& out);
Status parseEngineSnapshot(std::string_view text, EngineSnapshot& out);
Status parseCommandPreview(std::string_view text, CommandPreview& out);
Status parseZapretInfo(std::string_view text, ZapretInfo& out);
Status parseLogEntries(std::string_view text, std::vector<LogEntry>& out, std::uint64_t& lastSequence);
Status parseDiagnostics(std::string_view text, std::vector<Diagnostic>& out);

/// Extracts the error payload from an API error body, when present.
Status parseErrorPayload(std::string_view text, Error& out);

}
