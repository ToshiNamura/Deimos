#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace deimos
{

enum class Severity : std::uint8_t
{
    info = 0,
    warning = 1,
    error = 2,
};

std::string_view toString(Severity severity);

/// A single validation or inspection result about configuration or strategy.
struct Diagnostic
{
    Severity severity = Severity::info;
    /// Stable machine readable code, for example "profile.rounds.empty".
    std::string code;
    /// Human readable explanation shown by the CLI and the UI.
    std::string message;
    /// JSON pointer style location, for example "profiles[1].rounds[0].splitPos".
    std::string path;

    Diagnostic() = default;

    Diagnostic(Severity severityValue, std::string codeValue, std::string messageValue, std::string pathValue = {})
        : severity(severityValue)
        , code(std::move(codeValue))
        , message(std::move(messageValue))
        , path(std::move(pathValue))
    {
    }
};

inline Diagnostic info(std::string code, std::string message, std::string path = {})
{
    return Diagnostic{Severity::info, std::move(code), std::move(message), std::move(path)};
}

inline Diagnostic warning(std::string code, std::string message, std::string path = {})
{
    return Diagnostic{Severity::warning, std::move(code), std::move(message), std::move(path)};
}

inline Diagnostic error(std::string code, std::string message, std::string path = {})
{
    return Diagnostic{Severity::error, std::move(code), std::move(message), std::move(path)};
}

/// True when at least one diagnostic has error severity.
bool hasErrors(const std::vector<Diagnostic>& diagnostics);

/// Renders diagnostics as an indented multi-line report.
std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics);

}
