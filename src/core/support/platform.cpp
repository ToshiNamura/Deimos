#include <deimos/support/platform.hpp>

#include <cstdlib>
#include <fstream>
#include <system_error>

#include <deimos/support/strings.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#endif

namespace deimos::platform
{
namespace
{

std::filesystem::path homeDirectory()
{
#ifdef _WIN32
    if (const auto profile = environmentVariable("USERPROFILE"))
    {
        return std::filesystem::path{*profile};
    }
    return {};
#else
    if (const auto home = environmentVariable("HOME"))
    {
        return std::filesystem::path{*home};
    }
    const passwd* entry = getpwuid(getuid());
    if (entry != nullptr && entry->pw_dir != nullptr)
    {
        return std::filesystem::path{entry->pw_dir};
    }
    return {};
#endif
}

#ifdef _WIN32
std::filesystem::path fromWide(const std::wstring& wide)
{
    return std::filesystem::path{wide};
}

std::wstring toWidePath(const std::filesystem::path& path)
{
    return path.wstring();
}
#endif

} // namespace

std::filesystem::path configDirectory()
{
#ifdef _WIN32
    if (const auto appData = environmentVariable("APPDATA"))
    {
        return std::filesystem::path{*appData} / "Deimos";
    }
    return homeDirectory() / "AppData" / "Roaming" / "Deimos";
#elif defined(__APPLE__)
    return homeDirectory() / "Library" / "Application Support" / "Deimos";
#else
    if (const auto xdg = environmentVariable("XDG_CONFIG_HOME"))
    {
        if (!xdg->empty())
        {
            return std::filesystem::path{*xdg} / "deimos";
        }
    }
    return homeDirectory() / ".config" / "deimos";
#endif
}

std::filesystem::path stateDirectory()
{
#ifdef _WIN32
    if (const auto localAppData = environmentVariable("LOCALAPPDATA"))
    {
        return std::filesystem::path{*localAppData} / "Deimos";
    }
    return homeDirectory() / "AppData" / "Local" / "Deimos";
#elif defined(__APPLE__)
    return homeDirectory() / "Library" / "Application Support" / "Deimos";
#else
    if (const auto xdg = environmentVariable("XDG_STATE_HOME"))
    {
        if (!xdg->empty())
        {
            return std::filesystem::path{*xdg} / "deimos";
        }
    }
    return homeDirectory() / ".local" / "state" / "deimos";
#endif
}

std::filesystem::path systemStateDirectory()
{
#ifdef _WIN32
    if (const auto programData = environmentVariable("ProgramData"))
    {
        return std::filesystem::path{*programData} / "Deimos";
    }
    return std::filesystem::path{"C:\\ProgramData\\Deimos"};
#elif defined(__APPLE__)
    return std::filesystem::path{"/Library/Application Support/Deimos"};
#else
    return std::filesystem::path{"/var/lib/deimos"};
#endif
}

std::filesystem::path executablePath()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < buffer.size())
        {
            buffer.resize(length);
            return fromWide(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    char buffer[PATH_MAX] = {};
    return std::filesystem::path{buffer};
#else
    std::error_code code;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", code);
    if (code)
    {
        return {};
    }
    return path;
#endif
}

std::filesystem::path executableDirectory()
{
    const auto path = executablePath();
    if (path.empty())
    {
        return {};
    }
    return path.parent_path();
}

std::string hostname()
{
#ifdef _WIN32
    wchar_t buffer[256] = {};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (!GetComputerNameExW(ComputerNameDnsHostname, buffer, &size))
    {
        return "unknown";
    }
    return strings::toUtf8(std::wstring_view{buffer, size});
#else
    char buffer[256] = {};
    if (gethostname(buffer, sizeof(buffer)) != 0)
    {
        return "unknown";
    }
    return std::string{buffer};
#endif
}

std::string_view platformName()
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string_view architectureName()
{
#if defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}

std::optional<std::string> environmentVariable(std::string_view name)
{
#ifdef _WIN32
    const std::wstring wideName = strings::toWide(name);
    DWORD size = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
    if (size == 0)
    {
        return std::nullopt;
    }
    std::wstring buffer(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(wideName.c_str(), buffer.data(), size);
    if (written == 0)
    {
        return std::nullopt;
    }
    buffer.resize(written);
    return strings::toUtf8(buffer);
#else
    const std::string key{name};
    const char* value = std::getenv(key.c_str());
    if (value == nullptr)
    {
        return std::nullopt;
    }
    return std::string{value};
#endif
}

bool setEnvironmentVariable(std::string_view name, std::string_view value)
{
#ifdef _WIN32
    const std::wstring wideName = strings::toWide(name);
    const std::wstring wideValue = strings::toWide(value);
    return SetEnvironmentVariableW(wideName.c_str(), wideValue.c_str()) != 0;
#else
    const std::string key{name};
    const std::string text{value};
    return ::setenv(key.c_str(), text.c_str(), 1) == 0;
#endif
}

bool isElevated()
{
#ifdef _WIN32
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD returned = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned) != 0 &&
                          elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return elevated;
#else
    return geteuid() == 0;
#endif
}

bool isInteractiveSession()
{
#ifdef _WIN32
    return GetConsoleWindow() != nullptr;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

void ensureDirectory(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }
    std::error_code code;
    std::filesystem::create_directories(path, code);
}

bool writeFileAtomically(const std::filesystem::path& path, std::string_view content, std::string& errorMessage)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    if (!writeFile(temporary, content, errorMessage))
    {
        return false;
    }

#ifdef _WIN32
    const std::wstring from = toWidePath(temporary);
    const std::wstring to = toWidePath(path);
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        errorMessage = "cannot replace " + path.string() + " (error " + std::to_string(GetLastError()) + ")";
        std::error_code ignore;
        std::filesystem::remove(temporary, ignore);
        return false;
    }
#else
    std::error_code code;
    std::filesystem::rename(temporary, path, code);
    if (code)
    {
        errorMessage = "cannot replace " + path.string() + ": " + code.message();
        std::error_code ignore;
        std::filesystem::remove(temporary, ignore);
        return false;
    }
#endif
    return true;
}

std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream stream{path, std::ios::binary};
    if (!stream)
    {
        return std::nullopt;
    }
    std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        return std::nullopt;
    }
    return content;
}

bool writeFile(const std::filesystem::path& path, std::string_view content, std::string& errorMessage)
{
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream)
    {
        errorMessage = "cannot open " + path.string() + " for writing";
        return false;
    }
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    stream.flush();
    if (!stream)
    {
        errorMessage = "cannot write " + path.string();
        return false;
    }
    return true;
}

void restrictFilePermissions(const std::filesystem::path& path)
{
#ifndef _WIN32
    std::error_code code;
    std::filesystem::permissions(path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace,
                                 code);
#else
    (void)path;
#endif
}

std::optional<std::filesystem::path> findExecutable(std::string_view name)
{
    if (name.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path candidate{std::string{name}};
    std::error_code code;
    if (candidate.has_parent_path() && std::filesystem::is_regular_file(candidate, code))
    {
        return std::filesystem::absolute(candidate, code);
    }

#ifdef _WIN32
    std::wstring extensions = L".EXE;.CMD;.BAT;.COM";
    if (const auto pathExt = environmentVariable("PATHEXT"))
    {
        extensions = strings::toWide(*pathExt);
    }
    const auto extensionList = strings::split(strings::toUtf8(extensions), ';');

    const auto pathList = environmentVariable("PATH");
    if (!pathList)
    {
        return std::nullopt;
    }
    for (const std::string& directory : strings::split(*pathList, ';'))
    {
        if (directory.empty())
        {
            continue;
        }
        for (const std::string& extension : extensionList)
        {
            const std::string trimmedExtension = strings::trim(extension);
            if (trimmedExtension.empty())
            {
                continue;
            }
            std::filesystem::path full = std::filesystem::path{directory} / std::filesystem::path{std::string{name} + trimmedExtension};
            if (std::filesystem::is_regular_file(full, code))
            {
                return full;
            }
        }
    }
#else
    const auto pathList = environmentVariable("PATH");
    if (!pathList)
    {
        return std::nullopt;
    }
    for (const std::string& directory : strings::split(*pathList, ':'))
    {
        if (directory.empty())
        {
            continue;
        }
        std::filesystem::path full = std::filesystem::path{directory} / std::filesystem::path{std::string{name}};
        if (::access(full.c_str(), X_OK) == 0)
        {
            return full;
        }
    }
#endif

    return std::nullopt;
}

}
