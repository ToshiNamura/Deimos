#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/diagnostic.hpp>
#include <deimos/support/error.hpp>
#include <deimos/support/i18n.hpp>

namespace deimos
{

/// Diagnostics produced by the validation code are described by a stable code and
/// translated on creation, so the message follows the daemon language while the code
/// stays the machine readable contract.
///
/// The catalog lookup uses "diag." + code. A missing entry falls back to the code
/// itself instead of throwing, which keeps the daemon usable with an incomplete
/// catalog and makes the gap obvious in the output.
Diagnostic errorAt(std::string_view code, std::string path, std::initializer_list<std::string> arguments = {});

Diagnostic warningAt(std::string_view code, std::string path, std::initializer_list<std::string> arguments = {});

Diagnostic infoAt(std::string_view code, std::string path, std::initializer_list<std::string> arguments = {});

}
