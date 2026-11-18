#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <deimos/model/diagnostic.hpp>

namespace deimos::detail
{

using Json = nlohmann::json;

inline std::string jsonPath(std::string_view base, std::string_view key)
{
    if (base.empty())
    {
        return std::string{key};
    }
    return std::string{base} + "." + std::string{key};
}

inline std::vector<std::string> readStringArray(const Json& object, const char* key)
{
    std::vector<std::string> result;
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_array())
    {
        return result;
    }
    for (const Json& item : *iterator)
    {
        if (item.is_string())
        {
            result.push_back(item.get<std::string>());
        }
    }
    return result;
}

inline void writeStringArray(Json& object, const char* key, const std::vector<std::string>& values)
{
    if (!values.empty())
    {
        object[key] = values;
    }
}

template <typename Enum>
std::vector<Enum> readEnumArray(const Json& object,
                                const char* key,
                                bool (*parse)(std::string_view, Enum&),
                                std::string_view basePath,
                                std::vector<Diagnostic>& diagnostics)
{
    std::vector<Enum> result;
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_array())
    {
        return result;
    }

    for (const Json& item : *iterator)
    {
        if (!item.is_string())
        {
            diagnostics.push_back(error("config.invalid_value",
                                        std::string{"value of '"} + key + "' must be a string",
                                        jsonPath(basePath, key)));
            continue;
        }
        const std::string text = item.get<std::string>();
        Enum value{};
        if (parse(text, value))
        {
            result.push_back(value);
        }
        else
        {
            diagnostics.push_back(error("config.unknown_value",
                                        std::string{"unknown value '"} + text + "' for '" + key + "'",
                                        jsonPath(basePath, key)));
        }
    }
    return result;
}

template <typename Enum>
Json writeEnumArray(const std::vector<Enum>& values, std::string_view (*toToken)(Enum))
{
    Json array = Json::array();
    for (const Enum value : values)
    {
        array.push_back(std::string{toToken(value)});
    }
    return array;
}

inline std::vector<std::uint16_t> readPorts(const Json& object,
                                            const char* key,
                                            std::string_view basePath,
                                            std::vector<Diagnostic>& diagnostics)
{
    std::vector<std::uint16_t> ports;
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_array())
    {
        return ports;
    }

    for (const Json& item : *iterator)
    {
        if (!item.is_number_integer())
        {
            diagnostics.push_back(error("config.invalid_port",
                                        std::string{"port in '"} + key + "' must be an integer",
                                        jsonPath(basePath, key)));
            continue;
        }
        const std::int64_t value = item.get<std::int64_t>();
        if (value <= 0 || value > 65535)
        {
            diagnostics.push_back(error("config.invalid_port",
                                        "port " + std::to_string(value) + " is outside 1..65535",
                                        jsonPath(basePath, key)));
            continue;
        }
        ports.push_back(static_cast<std::uint16_t>(value));
    }
    return ports;
}

inline void writePorts(Json& object, const char* key, const std::vector<std::uint16_t>& ports)
{
    if (!ports.empty())
    {
        object[key] = ports;
    }
}

inline std::optional<int> readOptionalInt(const Json& object, const char* key)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || iterator->is_null())
    {
        return std::nullopt;
    }
    if (!iterator->is_number_integer())
    {
        return std::nullopt;
    }
    return iterator->get<int>();
}

inline void writeOptionalInt(Json& object, const char* key, const std::optional<int>& value)
{
    if (value.has_value())
    {
        object[key] = *value;
    }
}

}
