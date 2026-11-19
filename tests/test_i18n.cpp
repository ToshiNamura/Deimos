#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <deimos/model/config.hpp>
#include <deimos/model/diagnostic_i18n.hpp>
#include <deimos/support/i18n.hpp>
#include <deimos/support/labels.hpp>

using namespace deimos;

namespace
{

struct LanguageGuard
{
    LanguageGuard()
        : previous(i18n::current())
    {
    }

    ~LanguageGuard()
    {
        i18n::setLanguage(previous);
    }

    i18n::Language previous;
};

} // namespace

TEST_CASE("i18n_catalogsHaveIdenticalKeySets", "[i18n]")
{
    const std::vector<std::string> english = i18n::keys(i18n::Language::english);
    const std::vector<std::string> russian = i18n::keys(i18n::Language::russian);

    INFO("english keys: " << english.size() << ", russian keys: " << russian.size());
    REQUIRE(english.size() == russian.size());

    const std::set<std::string> englishSet(english.begin(), english.end());
    const std::set<std::string> russianSet(russian.begin(), russian.end());
    REQUIRE(englishSet == russianSet);
}

TEST_CASE("i18n_everyKeyResolvesInBothLanguages", "[i18n]")
{
    for (const std::string& key : i18n::keys(i18n::Language::english))
    {
        i18n::setLanguage(i18n::Language::english);
        const std::string english = i18n::tr(key);
        i18n::setLanguage(i18n::Language::russian);
        const std::string russian = i18n::tr(key);

        INFO("key: " << key);
        REQUIRE_FALSE(english.empty());
        REQUIRE_FALSE(russian.empty());
        REQUIRE(english != key);
        REQUIRE(russian != key);
    }
}

TEST_CASE("i18n_missingKeyFallsBackToTheKey", "[i18n]")
{
    LanguageGuard guard;
    i18n::setLanguage(i18n::Language::english);

    REQUIRE_FALSE(i18n::hasKey("does.not.exist"));
    REQUIRE(i18n::tr("does.not.exist") == "does.not.exist");
}

TEST_CASE("i18n_formatReplacesPlaceholdersInOrder", "[i18n]")
{
    LanguageGuard guard;

    i18n::setLanguage(i18n::Language::english);
    REQUIRE(i18n::tr("cli.status.processLine", {"tls-split", "4242"}) == "tls-split (pid 4242)");

    i18n::setLanguage(i18n::Language::russian);
    REQUIRE(i18n::tr("cli.error.invalidEndpoint", {"http://x", "bad port"}) ==
            "неверный адрес 'http://x': bad port");
}

TEST_CASE("i18n_parseLanguageAcceptsAliases", "[i18n]")
{
    REQUIRE(i18n::parseLanguage("ru") == i18n::Language::russian);
    REQUIRE(i18n::parseLanguage("RU") == i18n::Language::russian);
    REQUIRE(i18n::parseLanguage("russian") == i18n::Language::russian);
    REQUIRE(i18n::parseLanguage("en") == i18n::Language::english);
    REQUIRE(i18n::parseLanguage("en-GB") == i18n::Language::english);
    REQUIRE(i18n::parseLanguage("auto") == i18n::Language::system);
    REQUIRE(i18n::parseLanguage("") == i18n::Language::system);
    REQUIRE(i18n::parseLanguage("klingon") == i18n::Language::system);
    REQUIRE(i18n::code(i18n::Language::russian) == "ru");
    REQUIRE(i18n::code(i18n::Language::english) == "en");
}

TEST_CASE("i18n_switchingLanguageChangesOutput", "[i18n]")
{
    LanguageGuard guard;

    i18n::setLanguage(i18n::Language::english);
    const std::string english = i18n::tr("ui.action.start");

    i18n::setLanguage(i18n::Language::russian);
    const std::string russian = i18n::tr("ui.action.start");

    REQUIRE(english == "Start");
    REQUIRE(russian == "Запустить");
    REQUIRE(i18n::current() == i18n::Language::russian);
}

TEST_CASE("i18n_diagnosticsFollowTheSelectedLanguage", "[i18n]")
{
    LanguageGuard guard;

    i18n::setLanguage(i18n::Language::english);
    const Diagnostic english = errorAt("round.filter.empty", "rounds[0].filter");
    REQUIRE(english.code == "round.filter.empty");
    REQUIRE(english.path == "rounds[0].filter");
    REQUIRE(english.severity == Severity::error);
    REQUIRE(english.message == "a desync round needs at least one TCP or UDP port");

    i18n::setLanguage(i18n::Language::russian);
    const Diagnostic russian = errorAt("round.filter.empty", "rounds[0].filter");
    REQUIRE(russian.code == english.code);
    REQUIRE(russian.message == "в раунде нужен хотя бы один порт TCP или UDP");

    const Diagnostic formatted = warningAt("round.filter.duplicate_port", "p", {"TCP", "443"});
    REQUIRE(formatted.code == "round.filter.duplicate_port");
    REQUIRE(formatted.message.find("443") != std::string::npos);
    REQUIRE(formatted.message.find("TCP") != std::string::npos);
}

TEST_CASE("i18n_unknownDiagnosticCodeFallsBackToTheCode", "[i18n]")
{
    LanguageGuard guard;
    i18n::setLanguage(i18n::Language::english);

    const Diagnostic diagnostic = errorAt("no.such.code", "somewhere");
    REQUIRE(diagnostic.code == "no.such.code");
    REQUIRE(diagnostic.message == "diag.no.such.code");
}

TEST_CASE("i18n_labelsAreTranslated", "[i18n]")
{
    LanguageGuard guard;

    i18n::setLanguage(i18n::Language::english);
    REQUIRE(labels::engineState(EngineState::running) == "running");
    REQUIRE(labels::severity(Severity::warning) == "warning");
    REQUIRE(labels::duration(3725) == "1h 2m 5s");
    REQUIRE(labels::yesNo(true) == "yes");

    i18n::setLanguage(i18n::Language::russian);
    REQUIRE(labels::engineState(EngineState::running) == "работает");
    REQUIRE(labels::severity(Severity::warning) == "предупреждение");
    REQUIRE(labels::duration(3725) == "1 ч 2 мин 5 с");
    REQUIRE(labels::yesNo(false) == "нет");
}

TEST_CASE("config_languageIsPersistedAndValidated", "[i18n][config]")
{
    DeimosConfig config = defaultConfig();
    REQUIRE(config.daemon.language == "auto");

    const std::string text = serializeConfig(config, false);
    REQUIRE(text.find("\"language\":\"auto\"") != std::string::npos);

    DeimosConfig parsed;
    std::vector<Diagnostic> diagnostics;
    REQUIRE(deserializeConfig(text, parsed, diagnostics).isOk());
    REQUIRE(parsed.daemon.language == "auto");

    DeimosConfig broken = defaultConfig();
    broken.daemon.language = "klingon";
    const std::string brokenText = serializeConfig(broken, false);
    DeimosConfig rejected;
    diagnostics.clear();
    REQUIRE_FALSE(deserializeConfig(brokenText, rejected, diagnostics).isOk());
}
