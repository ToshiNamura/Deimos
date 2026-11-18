#include <deimos/support/strings.hpp>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace deimos::strings
{

std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    const auto isSpace = [](char character)
    {
        return character == ' ' || character == '\t' || character == '\r' || character == '\n' || character == '\f' ||
               character == '\v';
    };
    while (begin < end && isSpace(text[begin]))
    {
        ++begin;
    }
    while (end > begin && isSpace(text[end - 1]))
    {
        --end;
    }
    return std::string{text.substr(begin, end - begin)};
}

std::string toLower(std::string_view text)
{
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::string toUpper(std::string_view text)
{
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool equalsIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const auto a = static_cast<unsigned char>(left[index]);
        const auto b = static_cast<unsigned char>(right[index]);
        if (std::tolower(a) != std::tolower(b))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::string> split(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t position = text.find(separator, start);
        const std::size_t end = position == std::string_view::npos ? text.size() : position;
        parts.emplace_back(text.substr(start, end - start));
        if (position == std::string_view::npos)
        {
            break;
        }
        start = position + 1;
    }
    return parts;
}

std::vector<std::string> splitTrimmed(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    for (const std::string& part : split(text, separator))
    {
        std::string trimmed = trim(part);
        if (!trimmed.empty())
        {
            parts.push_back(std::move(trimmed));
        }
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator)
{
    std::string result;
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index > 0)
        {
            result.append(separator);
        }
        result.append(parts[index]);
    }
    return result;
}

std::string replaceAll(std::string_view text, std::string_view from, std::string_view to)
{
    if (from.empty())
    {
        return std::string{text};
    }
    std::string result;
    result.reserve(text.size());
    std::size_t position = 0;
    while (position < text.size())
    {
        const std::size_t found = text.find(from, position);
        if (found == std::string_view::npos)
        {
            result.append(text.substr(position));
            break;
        }
        result.append(text.substr(position, found - position));
        result.append(to);
        position = found + from.size();
    }
    return result;
}

std::optional<std::vector<std::uint16_t>> parsePortList(std::string_view text)
{
    std::vector<std::uint16_t> ports;
    const std::string trimmedText = trim(text);
    if (trimmedText.empty())
    {
        return ports;
    }

    for (const std::string& token : splitTrimmed(trimmedText, ','))
    {
        std::int64_t value = 0;
        if (!parseInt(token, value))
        {
            return std::nullopt;
        }
        if (value <= 0 || value > 65535)
        {
            return std::nullopt;
        }
        ports.push_back(static_cast<std::uint16_t>(value));
    }
    return ports;
}

bool parseInt(std::string_view text, std::int64_t& out)
{
    const std::string trimmedText = trim(text);
    if (trimmedText.empty())
    {
        return false;
    }
    std::int64_t value = 0;
    const char* begin = trimmedText.data();
    const char* end = begin + trimmedText.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return false;
    }
    out = value;
    return true;
}

bool parseBool(std::string_view text, bool& out)
{
    const std::string value = toLower(trim(text));
    if (value == "true" || value == "yes" || value == "on" || value == "1")
    {
        out = true;
        return true;
    }
    if (value == "false" || value == "no" || value == "off" || value == "0")
    {
        out = false;
        return true;
    }
    return false;
}

std::string toHex(std::string_view bytes)
{
    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const char character : bytes)
    {
        const auto value = static_cast<unsigned char>(character);
        result.push_back(DIGITS[value >> 4]);
        result.push_back(DIGITS[value & 0x0F]);
    }
    return result;
}

std::optional<std::string> fromHex(std::string_view hex)
{
    const auto hexValue = [](char character) -> int
    {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }
        return -1;
    };

    if (hex.size() % 2 != 0)
    {
        return std::nullopt;
    }

    std::string result;
    result.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2)
    {
        const int high = hexValue(hex[index]);
        const int low = hexValue(hex[index + 1]);
        if (high < 0 || low < 0)
        {
            return std::nullopt;
        }
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return result;
}

std::string quoteForDisplay(std::string_view text)
{
    const bool needsQuotes = text.empty() || text.find_first_of(" \t\"'") != std::string_view::npos;
    if (!needsQuotes)
    {
        return std::string{text};
    }

    std::string result = "\"";
    for (const char character : text)
    {
        if (character == '"' || character == '\\')
        {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    result.push_back('"');
    return result;
}

#ifdef _WIN32

std::wstring toWide(std::string_view utf8)
{
    if (utf8.empty())
    {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0)
    {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
    return wide;
}

std::string toUtf8(std::wstring_view wide)
{
    if (wide.empty())
    {
        return {};
    }
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0)
    {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr);
    return utf8;
}

#else

std::wstring toWide(std::string_view)
{
    return {};
}

std::string toUtf8(std::wstring_view)
{
    return {};
}

#endif

}
