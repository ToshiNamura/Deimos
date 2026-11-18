#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/model/desync.hpp>
#include <deimos/model/diagnostic.hpp>
#include <deimos/support/error.hpp>

namespace deimos
{

/// Executables and support directories of a zapret installation.
struct ToolPaths
{
    std::filesystem::path winws;
    std::filesystem::path nfqws;
    std::filesystem::path tpws;

    /// Root of the zapret tree, when it could be identified.
    std::filesystem::path baseDirectory;
    /// Directory holding fake payload files (zapret files/).
    std::filesystem::path filesDirectory;
    /// Directory holding hostlist and ipset files (zapret lists/).
    std::filesystem::path listsDirectory;

    bool has(ToolKind tool) const;
    std::filesystem::path pathFor(ToolKind tool) const;
    bool any() const;
};

struct ToolchainReport
{
    Status status;
    ToolPaths paths;
    std::vector<Diagnostic> diagnostics;
};

/// Resolves the zapret toolchain from explicit paths, then an install directory, then auto-detection.
ToolchainReport detectZapret(const ZapretSettings& settings);

/// The tool Deimos uses on the current platform when a profile says "auto".
ToolKind platformDefaultTool();

/// The executable name zapret uses on the current platform.
std::string_view platformToolExecutableName(ToolKind tool);

/// Picks the tool a profile should run. Returns an error when the requested tool is unavailable.
Result<ToolKind> resolveTool(ToolKind requested, const ToolPaths& paths);

/// Builds a human readable explanation of where Deimos looked for zapret.
std::vector<std::string> zapretSearchRoots(const ZapretSettings& settings);

}
