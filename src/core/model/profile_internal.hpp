#pragma once

#include <string_view>
#include <vector>

#include <deimos/model/diagnostic.hpp>
#include <deimos/model/profile.hpp>

#include "model_json.hpp"

namespace deimos::detail
{

Profile parseProfileJson(const Json& node, std::string_view path, std::vector<Diagnostic>& diagnostics);

Json writeProfileJson(const Profile& profile);

}
