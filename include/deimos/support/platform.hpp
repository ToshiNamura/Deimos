#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace deimos::platform
{
/// Per-user directory for configuration (roaming on Windows, ~/.config on Linux).
std::filesystem::path configDirectory();

/// Per-user directory for mutable state: logs, pid files, generated tokens.
std::filesystem::path stateDirectory();

/// Per-machine directory used for system wide installs.
std::filesystem::path systemStateDirectory();

/// Absolute path of the running executable, empty when it cannot be resolved.
std::filesystem::path executablePath();

/// Path of the directory holding the running executable.
std::filesystem::path executableDirectory();

std::string hostname();

/// "windows", "linux", "macos" or "unknown".
std::string_view platformName();

/// "x86_64", "arm64", "x86" or "unknown".
std::string_view architectureName();

std::optional<std::string> environmentVariable(std::string_view name);

bool setEnvironmentVariable(std::string_view name, std::string_view value);

/// True when the process has administrator/root rights.
bool isElevated();

/// True when the process is attached to an interactive console/window station.
bool isInteractiveSession();

/// Creates the directory and parents when missing.
void ensureDirectory(const std::filesystem::path& path);

/// Writes \p content to \p path atomically (temporary file + rename).
bool writeFileAtomically(const std::filesystem::path& path, std::string_view content, std::string& errorMessage);

std::optional<std::string> readFile(const std::filesystem::path& path);

bool writeFile(const std::filesystem::path& path, std::string_view content, std::string& errorMessage);

/// Restricts a secret file to the current user where the platform supports it.
void restrictFilePermissions(const std::filesystem::path& path);

/// Locates an executable on PATH, honouring PATHEXT on Windows.
std::optional<std::filesystem::path> findExecutable(std::string_view name);

}
