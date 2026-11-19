#include <deimos/zapret/toolchain.hpp>

#include <algorithm>
#include <optional>

#include <deimos/model/diagnostic_i18n.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

constexpr int MAX_SCAN_DEPTH = 4;
constexpr std::size_t MAX_SCAN_ENTRIES = 4000;

std::string toolFileName(std::string_view base)
{
    std::string name{base};
#ifdef _WIN32
    name += ".exe";
#endif
    return name;
}

std::filesystem::path probeDirectory(const std::filesystem::path& directory, std::string_view toolBase)
{
    if (directory.empty())
    {
        return {};
    }
    std::error_code code;
    const std::filesystem::path candidate = directory / toolFileName(toolBase);
    if (std::filesystem::is_regular_file(candidate, code))
    {
        return candidate;
    }
    return {};
}

std::filesystem::path scanForTool(const std::filesystem::path& root, std::string_view toolBase)
{
    std::error_code code;
    if (root.empty() || !std::filesystem::is_directory(root, code))
    {
        return {};
    }

    const std::string fileName = strings::toLower(toolFileName(toolBase));
    std::size_t budget = MAX_SCAN_ENTRIES;
    std::filesystem::recursive_directory_iterator iterator{root,
                                                           std::filesystem::directory_options::skip_permission_denied,
                                                           code};
    const std::filesystem::recursive_directory_iterator end;
    for (; iterator != end && budget > 0; iterator.increment(code))
    {
        if (code)
        {
            break;
        }
        --budget;
        if (iterator.depth() >= MAX_SCAN_DEPTH)
        {
            iterator.disable_recursion_pending();
            continue;
        }
        if (!iterator->is_regular_file(code))
        {
            continue;
        }
        if (strings::toLower(iterator->path().filename().string()) == fileName)
        {
            return iterator->path();
        }
    }
    return {};
}

std::filesystem::path findTool(const std::filesystem::path& base, std::string_view toolBase)
{
    if (base.empty())
    {
        return {};
    }

    if (auto found = probeDirectory(base / "bin", toolBase); !found.empty())
    {
        return found;
    }
    if (auto found = probeDirectory(base, toolBase); !found.empty())
    {
        return found;
    }

    std::error_code code;
    const std::filesystem::path binaries = base / "binaries";
    if (std::filesystem::is_directory(binaries, code))
    {
        for (std::filesystem::directory_iterator iterator{binaries, code}, end; iterator != end;
             iterator.increment(code))
        {
            if (code)
            {
                break;
            }
            if (auto found = probeDirectory(iterator->path(), toolBase); !found.empty())
            {
                return found;
            }
        }
    }

    return scanForTool(base, toolBase);
}

std::vector<std::filesystem::path> candidateRoots(const ZapretSettings& settings)
{
    std::vector<std::filesystem::path> roots;
    const auto push = [&roots](const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return;
        }
        const auto normalized = path.lexically_normal();
        if (std::find(roots.begin(), roots.end(), normalized) == roots.end())
        {
            roots.push_back(normalized);
        }
    };

    if (!settings.installDirectory.empty())
    {
        push(std::filesystem::path{settings.installDirectory});
    }
    if (const auto fromEnvironment = platform::environmentVariable("DEIMOS_ZAPRET_DIR"))
    {
        push(std::filesystem::path{*fromEnvironment});
    }
    if (const auto executableDir = platform::executableDirectory(); !executableDir.empty())
    {
        push(executableDir / "zapret");
        push(executableDir / ".." / "zapret");
        push(executableDir);
    }
    std::error_code workingDirectoryCode;
    const auto workingDirectory = std::filesystem::current_path(workingDirectoryCode);
    if (!workingDirectory.empty())
    {
        push(workingDirectory / "zapret");
        push(workingDirectory);
    }
    if (const auto programData = platform::environmentVariable("ProgramData"))
    {
        push(std::filesystem::path{*programData} / "zapret");
    }
    if (const auto programFiles = platform::environmentVariable("ProgramFiles"))
    {
        push(std::filesystem::path{*programFiles} / "zapret");
    }
    push(std::filesystem::path{"/opt/zapret"});
    push(std::filesystem::path{"/usr/local/zapret"});

    return roots;
}

} // namespace

bool ToolPaths::has(ToolKind tool) const
{
    return !pathFor(tool).empty();
}

std::filesystem::path ToolPaths::pathFor(ToolKind tool) const
{
    switch (tool)
    {
    case ToolKind::winws:
        return winws;
    case ToolKind::nfqws:
        return nfqws;
    case ToolKind::tpws:
        return tpws;
    case ToolKind::autoDetect:
        break;
    }
    return {};
}

bool ToolPaths::any() const
{
    return !winws.empty() || !nfqws.empty() || !tpws.empty();
}

ToolKind platformDefaultTool()
{
#ifdef _WIN32
    return ToolKind::winws;
#else
    return ToolKind::nfqws;
#endif
}

std::string_view platformToolExecutableName(ToolKind tool)
{
    switch (tool)
    {
    case ToolKind::winws:
        return "winws";
    case ToolKind::nfqws:
        return "nfqws";
    case ToolKind::tpws:
        return "tpws";
    case ToolKind::autoDetect:
        break;
    }
    return "nfqws";
}

std::vector<std::string> zapretSearchRoots(const ZapretSettings& settings)
{
    std::vector<std::string> roots;
    for (const auto& root : candidateRoots(settings))
    {
        roots.push_back(root.string());
    }
    return roots;
}

ToolchainReport detectZapret(const ZapretSettings& settings)
{
    ToolchainReport report;
    std::vector<Diagnostic> diagnostics;

    const auto overrideTool = [&diagnostics](const std::string& configured,
                                             const char* field,
                                             std::filesystem::path& target)
    {
        if (configured.empty())
        {
            return;
        }
        std::error_code code;
        const std::filesystem::path candidate{configured};
        if (std::filesystem::is_regular_file(candidate, code))
        {
            target = std::filesystem::absolute(candidate, code);
            return;
        }
        if (const auto onPath = platform::findExecutable(configured))
        {
            target = *onPath;
            return;
        }
        diagnostics.push_back(warningAt("zapret.override.missing",
                                        std::string{"zapret."} + field,
                                        {std::string{field}, configured}));
    };

    overrideTool(settings.winwsPath, "winwsPath", report.paths.winws);
    overrideTool(settings.nfqwsPath, "nfqwsPath", report.paths.nfqws);
    overrideTool(settings.tpwsPath, "tpwsPath", report.paths.tpws);

    std::filesystem::path base;
    if (!settings.installDirectory.empty())
    {
        std::error_code code;
        const std::filesystem::path configured{settings.installDirectory};
        if (std::filesystem::is_directory(configured, code))
        {
            base = std::filesystem::absolute(configured, code);
        }
        else
        {
            diagnostics.push_back(errorAt("zapret.install_directory.missing",
                                          "zapret.installDirectory",
                                          {settings.installDirectory}));
        }
    }
    else
    {
        for (const auto& root : candidateRoots(settings))
        {
            std::error_code code;
            if (!std::filesystem::is_directory(root, code))
            {
                continue;
            }
            const bool hasAnyTool = !findTool(root, "winws").empty() || !findTool(root, "nfqws").empty() ||
                                    !findTool(root, "tpws").empty();
            if (hasAnyTool)
            {
                base = root;
                break;
            }
        }
    }

    if (!base.empty())
    {
        report.paths.baseDirectory = base;

        if (report.paths.winws.empty())
        {
            report.paths.winws = findTool(base, "winws");
        }
        if (report.paths.nfqws.empty())
        {
            report.paths.nfqws = findTool(base, "nfqws");
        }
        if (report.paths.tpws.empty())
        {
            report.paths.tpws = findTool(base, "tpws");
        }

        std::error_code code;
        const std::filesystem::path files = base / "files";
        if (std::filesystem::is_directory(files, code))
        {
            report.paths.filesDirectory = files;
        }
        const std::filesystem::path lists = base / "lists";
        if (std::filesystem::is_directory(lists, code))
        {
            report.paths.listsDirectory = lists;
        }
    }

    if (!report.paths.winws.empty())
    {
        diagnostics.push_back(infoAt("zapret.tool.found", "", {"winws", report.paths.winws.string()}));
    }
    if (!report.paths.nfqws.empty())
    {
        diagnostics.push_back(infoAt("zapret.tool.found", "", {"nfqws", report.paths.nfqws.string()}));
    }
    if (!report.paths.tpws.empty())
    {
        diagnostics.push_back(infoAt("zapret.tool.found", "", {"tpws", report.paths.tpws.string()}));
    }

    if (!report.paths.any())
    {
        std::string searched;
        for (const std::string& root : zapretSearchRoots(settings))
        {
            if (!searched.empty())
            {
                searched += ", ";
            }
            searched += root;
        }
        diagnostics.push_back(
            errorAt("zapret.not_found", "zapret.installDirectory", {searched}));
        report.status = Status::failure(ErrorCode::notFound,
                                        i18n::tr("err.zapret.not_found"),
                                        i18n::tr("err.zapret.not_found.detail"));
    }
    else
    {
        report.status = Status::ok();
    }

    report.diagnostics = std::move(diagnostics);
    return report;
}

Result<ToolKind> resolveTool(ToolKind requested, const ToolPaths& paths)
{
    if (requested != ToolKind::autoDetect)
    {
        if (paths.has(requested))
        {
            return Result<ToolKind>{requested};
        }
        return Result<ToolKind>{Error{ErrorCode::notFound,
                                      "the requested zapret tool is not available",
                                      std::string{"tool: "} + std::string{toCliToken(requested)}}};
    }

    const ToolKind preferred = platformDefaultTool();
    if (paths.has(preferred))
    {
        return Result<ToolKind>{preferred};
    }
    for (const ToolKind candidate : allToolKinds())
    {
        if (candidate != ToolKind::autoDetect && paths.has(candidate))
        {
            return Result<ToolKind>{candidate};
        }
    }
    return Result<ToolKind>{Error{ErrorCode::notFound,
                                  "no zapret tool is available on this machine",
                                  "install zapret or set zapret.installDirectory"}};
}

}
