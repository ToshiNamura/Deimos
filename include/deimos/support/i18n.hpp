#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace deimos::i18n
{

enum class Language : unsigned char
{
    system = 0,
    english,
    russian,
};

/// Language selected from the operating system, falling back to English.
Language detectSystemLanguage();

/// Parses "auto"/"system", "en"/"english", "ru"/"russian" case insensitively.
Language parseLanguage(std::string_view text, Language fallback = Language::system);

/// "auto", "en" or "ru".
std::string_view code(Language language);

/// Turns `system` into the detected language.
Language resolve(Language language);

Language current();

/// Selecting `system` switches to the detected language immediately.
void setLanguage(Language language);

/// Convenience wrapper that ignores unknown values.
void setLanguageByCode(std::string_view text);

bool hasKey(std::string_view key);

/// Translated text for \p key, or the key itself when the catalog has no entry.
const std::string& tr(std::string_view key);

/// Translated text with `{0}`, `{1}`, ... replaced by \p arguments in order.
std::string tr(std::string_view key, std::initializer_list<std::string> arguments);

/// Sorted keys of a catalog, exposed for tests that check EN/RU parity.
std::vector<std::string> keys(Language language);

}
