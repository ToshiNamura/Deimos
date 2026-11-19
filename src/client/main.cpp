#include <cstdio>
#include <cstring>
#include <iostream>

#include <deimos/support/i18n.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(DEIMOS_HAS_UI)
namespace deimos::ui
{
int runWindow(const std::string& languageOverride);
}
#endif

#if defined(DEIMOS_HAS_CLI)
namespace deimos
{
int runCli(int argc, char** argv);
}
#endif

namespace
{

constexpr int EXIT_USAGE = 2;

bool hasFlag(int argc, char** argv, const char* flag)
{
    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], flag) == 0)
        {
            return true;
        }
    }
    return false;
}

/// True when a terminal (or a redirect) is attached to this process.
///
/// The binary is a console application: that keeps pipes, exit codes and blocking
/// behaviour correct for scripting. On Windows a double click makes the system
/// create a console window, which we detect by checking that we are its only user.
bool launchedFromTerminal()
{
#ifdef _WIN32
    DWORD processes[2] = {};
    const DWORD count = GetConsoleProcessList(processes, 2);
    return count > 1;
#else
    return isatty(STDOUT_FILENO) != 0 || isatty(STDERR_FILENO) != 0;
#endif
}

bool wantsWindow(int argc, char** argv, bool fromTerminal)
{
    if (hasFlag(argc, argv, "--gui"))
    {
        return true;
    }
    if (argc > 1)
    {
        return false;
    }

    // Without arguments a desktop launch opens the window, a shell invocation prints help.
    return !fromTerminal;
}

void detachConsoleForWindow()
{
#ifdef _WIN32
    FreeConsole();
#endif
}

void prepareConsole()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

} // namespace

int main(int argc, char** argv)
{
    const bool fromTerminal = launchedFromTerminal();

    // Language must be set before the window opens; the CLI does the same inside runCli().
    std::string languageOverride;
    for (int index = 1; index + 1 < argc; ++index)
    {
        if (std::strcmp(argv[index], "--lang") == 0)
        {
            languageOverride = argv[index + 1];
            deimos::i18n::setLanguageByCode(languageOverride);
            break;
        }
    }

    if (wantsWindow(argc, argv, fromTerminal))
    {
        detachConsoleForWindow();
#if defined(DEIMOS_HAS_UI)
        return deimos::ui::runWindow(languageOverride);
#else
        std::fprintf(stderr, "deimos: this build does not include the graphical interface\n");
        return EXIT_USAGE;
#endif
    }

#if defined(DEIMOS_HAS_CLI)
    prepareConsole();
    const int exitCode = deimos::runCli(argc, argv);
    std::cout.flush();
    std::cerr.flush();
    return exitCode;
#else
    std::fprintf(stderr, "deimos: this build does not include the command line client\n");
    return EXIT_USAGE;
#endif
}
