#include <deimos/model/diagnostic.hpp>

#include <sstream>

namespace deimos
{

std::string_view toString(Severity severity)
{
    switch (severity)
    {
    case Severity::info:
        return "info";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    }
    return "info";
}

bool hasErrors(const std::vector<Diagnostic>& diagnostics)
{
    for (const Diagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == Severity::error)
        {
            return true;
        }
    }
    return false;
}

std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics)
{
    std::ostringstream stream;
    for (const Diagnostic& diagnostic : diagnostics)
    {
        stream << "  " << toString(diagnostic.severity) << ": " << diagnostic.message;
        if (!diagnostic.path.empty())
        {
            stream << "  [" << diagnostic.path << "]";
        }
        stream << '\n';
    }
    return stream.str();
}

}
