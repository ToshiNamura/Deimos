#include <deimos/support/i18n.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>

namespace deimos::i18n
{
namespace
{

struct Entry
{
    const char* key;
    const char* text;
};

#include "i18n_catalog.inc"

std::unordered_map<std::string, std::string> makeCatalog(const Entry* entries, std::size_t count)
{
    std::unordered_map<std::string, std::string> catalog;
    catalog.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        catalog.emplace(entries[index].key, entries[index].text);
    }
    return catalog;
}

const std::unordered_map<std::string, std::string>& catalogFor(Language language)
{
    static const auto english = makeCatalog(ENGLISH_ENTRIES, sizeof(ENGLISH_ENTRIES) / sizeof(Entry));
    static const auto russian = makeCatalog(RUSSIAN_ENTRIES, sizeof(RUSSIAN_ENTRIES) / sizeof(Entry));
    return language == Language::russian ? russian : english;
}

std::atomic<Language> g_language{Language::english};

Language languageFromTag(std::string_view tag)
{
    if (tag.size() >= 2 && strings::equalsIgnoreCase(tag.substr(0, 2), "ru"))
    {
        return Language::russian;
    }
    return Language::english;
}

std::string replacePlaceholders(std::string text, std::initializer_list<std::string> arguments)
{
    std::size_t index = 0;
    for (const std::string& argument : arguments)
    {
        const std::string placeholder = "{" + std::to_string(index) + "}";
        std::string::size_type position = 0;
        while ((position = text.find(placeholder, position)) != std::string::npos)
        {
            text.replace(position, placeholder.size(), argument);
            position += argument.size();
        }
        ++index;
    }
    return text;
}

} // namespace

Language detectSystemLanguage()
{
    if (const auto fromEnvironment = platform::environmentVariable("LC_ALL"); fromEnvironment && !fromEnvironment->empty())
    {
        return languageFromTag(*fromEnvironment);
    }
    if (const auto fromEnvironment = platform::environmentVariable("LC_MESSAGES");
        fromEnvironment && !fromEnvironment->empty())
    {
        return languageFromTag(*fromEnvironment);
    }
    if (const auto fromEnvironment = platform::environmentVariable("LANG"); fromEnvironment && !fromEnvironment->empty())
    {
        return languageFromTag(*fromEnvironment);
    }

#ifdef _WIN32
    const LANGID langId = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(langId) == LANG_RUSSIAN)
    {
        return Language::russian;
    }
#endif

    return Language::english;
}

Language parseLanguage(std::string_view text, Language fallback)
{
    std::string value = strings::toLower(strings::trim(text));
    if (value.empty() || value == "auto" || value == "system")
    {
        return fallback;
    }

    // Accept full language tags such as "ru-RU" or "en_GB".
    const std::size_t separator = value.find_first_of("-_");
    if (separator != std::string::npos)
    {
        value.resize(separator);
    }

    if (value == "en" || value == "eng" || value == "english")
    {
        return Language::english;
    }
    if (value == "ru" || value == "rus" || value == "russian")
    {
        return Language::russian;
    }
    return fallback;
}

std::string_view code(Language language)
{
    switch (resolve(language))
    {
    case Language::russian:
        return "ru";
    case Language::english:
    case Language::system:
        break;
    }
    return "en";
}

Language resolve(Language language)
{
    if (language != Language::system)
    {
        return language;
    }
    if (const auto fromEnvironment = platform::environmentVariable("DEIMOS_LANG"); fromEnvironment)
    {
        const Language parsed = parseLanguage(*fromEnvironment, Language::system);
        if (parsed != Language::system)
        {
            return parsed;
        }
    }
    return detectSystemLanguage();
}

Language current()
{
    return g_language.load(std::memory_order_relaxed);
}

void setLanguage(Language language)
{
    g_language.store(resolve(language), std::memory_order_relaxed);
}

void setLanguageByCode(std::string_view text)
{
    const Language parsed = parseLanguage(text, Language::system);
    setLanguage(parsed);
}

bool hasKey(std::string_view key)
{
    const auto& catalog = catalogFor(current());
    return catalog.find(std::string{key}) != catalog.end();
}

const std::string& tr(std::string_view key)
{
    const auto& catalog = catalogFor(current());
    const auto iterator = catalog.find(std::string{key});
    if (iterator != catalog.end())
    {
        return iterator->second;
    }

    static thread_local std::string fallback;
    fallback.assign(key);
    return fallback;
}

std::string tr(std::string_view key, std::initializer_list<std::string> arguments)
{
    const std::string text = tr(key);
    return replacePlaceholders(text, arguments);
}

std::vector<std::string> keys(Language language)
{
    std::vector<std::string> result;
    for (const auto& entry : catalogFor(resolve(language)))
    {
        result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

}
