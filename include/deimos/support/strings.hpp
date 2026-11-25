#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deimos::strings
{

std::string trim(std::string_view text);
std::string toLower(std::string_view text);
std::string toUpper(std::string_view text);

bool startsWith(std::string_view text, std::string_view prefix);
bool endsWith(std::string_view text, std::string_view suffix);
bool equalsIgnoreCase(std::string_view left, std::string_view right);

std::vector<std::string> split(std::string_view text, char separator);
std::vector<std::string> splitTrimmed(std::string_view text, char separator);
std::string join(const std::vector<std::string>& parts, std::string_view separator);

std::string replaceAll(std::string_view text, std::string_view from, std::string_view to);

/// Formats the host-and-port part of an endpoint URL, bracketing IPv6 literals.
std::string formatEndpointUrl(std::string_view host, std::uint16_t port);

/// Splits "80,443" or " 80 , 443 " into numbers. Returns an empty vector on malformed input.
std::optional<std::vector<std::uint16_t>> parsePortList(std::string_view text);

bool parseInt(std::string_view text, std::int64_t& out);
bool parseBool(std::string_view text, bool& out);

std::string toHex(std::string_view bytes);
std::optional<std::string> fromHex(std::string_view hex);

/// Escapes control characters so a command line can be logged on one line.
std::string quoteForDisplay(std::string_view text);

/// Converts UTF-8 <-> UTF-16. Non-Windows builds return an empty string.
std::wstring toWide(std::string_view utf8);
std::string toUtf8(std::wstring_view wide);

}
