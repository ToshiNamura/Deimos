#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/model/desync.hpp>
#include <deimos/model/diagnostic.hpp>
#include <deimos/zapret/toolchain.hpp>

namespace deimos
{

/// One process the engine has to spawn for a profile.
struct LaunchSpec
{
    std::string executable;
    std::vector<std::string> arguments;
    std::string workingDirectory;

    /// Short label used in logs and UI, for example "tls-split [tcp 443]".
    std::string label;

    /// Shell-style rendering, for display and copy/paste only.
    std::string renderCommandLine() const;
};

struct BuildResult
{
    std::vector<LaunchSpec> launches;
    std::vector<Diagnostic> diagnostics;

    bool hasErrors() const
    {
        return deimos::hasErrors(diagnostics);
    }
};

/// Translates a profile into concrete zapret command lines.
///
/// winws accepts every round in a single process separated by --new, while nfqws and
/// tpws get one process per round because they only handle one rule set per invocation.
BuildResult buildLaunches(const Profile& profile,
                          ToolKind tool,
                          const ToolPaths& paths,
                          std::string_view workingDirectory);

/// Convenience overload that resolves the tool from the platform first.
BuildResult buildLaunchesForPlatform(const Profile& profile, const ToolPaths& paths, std::string_view workingDirectory);

}
