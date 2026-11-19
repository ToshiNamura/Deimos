#include <deimos/model/diagnostic_i18n.hpp>

#include <utility>

namespace deimos
{
namespace
{

Diagnostic makeLocalized(Severity severity,
                         std::string_view diagnosticCode,
                         std::string path,
                         std::initializer_list<std::string> arguments)
{
    const std::string key = std::string{"diag."} + std::string{diagnosticCode};
    return Diagnostic{severity, std::string{diagnosticCode}, i18n::tr(key, arguments), std::move(path)};
}

} // namespace

Diagnostic errorAt(std::string_view code, std::string path, std::initializer_list<std::string> arguments)
{
    return makeLocalized(Severity::error, code, std::move(path), arguments);
}

Diagnostic warningAt(std::string_view code, std::string path, std::initializer_list<std::string> arguments)
{
    return makeLocalized(Severity::warning, code, std::move(path), arguments);
}

Diagnostic infoAt(std::string_view code, std::string path, std::initializer_list<std::string> arguments)
{
    return makeLocalized(Severity::info, code, std::move(path), arguments);
}

}
