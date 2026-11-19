#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <deimos/model/desync.hpp>
#include <deimos/model/diagnostic.hpp>
#include <deimos/support/i18n.hpp>
#include <deimos/support/log.hpp>
#include <deimos/transport/protocol.hpp>

/// Localised labels shared by the CLI and the desktop interface, so both present
/// the same wording for states, levels and durations.
namespace deimos::labels
{

std::string_view severity(Severity value);
std::string_view engineState(EngineState value);
std::string_view logLevel(LogLevel value);
std::string_view tool(ToolKind value);
std::string_view desyncMode(DesyncMode value);
std::string_view fooling(Fooling value);
std::string_view l7(L7Protocol value);
std::string_view languageName(i18n::Language value);

/// "2h 5m 3s" / "2 ч 5 мин 3 с".
std::string duration(std::int64_t seconds);
std::string yesNo(bool value);

}
