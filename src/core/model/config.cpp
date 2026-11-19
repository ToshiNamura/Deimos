#include <deimos/model/config.hpp>

#include <deimos/model/diagnostic_i18n.hpp>

#include <exception>
#include <utility>

#include <deimos/support/i18n.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>

#include "model_json.hpp"
#include "profile_internal.hpp"

namespace deimos
{
namespace
{

Profile makeTlsSplitProfile()
{
    Profile profile;
    profile.id = "tls-split";
    profile.name = "TLS split";
    profile.description =
        "Splits the TLS ClientHello so DPI engines cannot read the SNI in a single segment.";
    profile.tool = ToolKind::autoDetect;

    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.filter.l7 = {L7Protocol::tls};
    round.modes = {DesyncMode::split2};
    round.splitPos = 1;
    round.autoTtl = 2;

    profile.rounds.push_back(round);
    return profile;
}

Profile makeQuicSplitProfile()
{
    Profile profile;
    profile.id = "quic-split";
    profile.name = "QUIC fake initial";
    profile.description =
        "Answers QUIC Initial packets with a fake copy so blocked video streams fall back to TCP.";
    profile.tool = ToolKind::autoDetect;

    DesyncRound round;
    round.filter.udpPorts = {443};
    round.filter.l7 = {L7Protocol::quic};
    round.modes = {DesyncMode::fake};
    round.repeats = 6;
    round.fooling = {Fooling::md5sig};
    round.fakePayload = FakePayload{FakePayloadKind::file, "files/fake/quic_initial_www_google_com.bin"};

    profile.rounds.push_back(round);
    return profile;
}

detail::Json writeDaemonSettings(const DaemonSettings& settings)
{
    detail::Json node{
        {"listenAddress", settings.listenAddress},
        {"port", settings.port},
        {"language", settings.language},
        {"allowRemoteClients", settings.allowRemoteClients},
        {"requireAuthentication", settings.requireAuthentication},
        {"logLevel", std::string{toString(settings.logLevel)}},
        {"logHistorySize", settings.logHistorySize},
        {"autoStartEngine", settings.autoStartEngine},
        {"activeProfileId", settings.activeProfileId},
        {"enableTls", settings.enableTls},
    };
    if (!settings.tokenFile.empty())
    {
        node["tokenFile"] = settings.tokenFile;
    }
    if (!settings.tlsCertificateFile.empty())
    {
        node["tlsCertificateFile"] = settings.tlsCertificateFile;
    }
    if (!settings.tlsPrivateKeyFile.empty())
    {
        node["tlsPrivateKeyFile"] = settings.tlsPrivateKeyFile;
    }
    return node;
}

detail::Json writeEngineSettings(const EngineSettings& settings)
{
    return detail::Json{
        {"maxRestarts", settings.maxRestarts},
        {"restartBackoffMs", settings.restartBackoffMs},
        {"stopTimeoutMs", settings.stopTimeoutMs},
        {"restartOnCrash", settings.restartOnCrash},
    };
}

detail::Json writeZapretSettings(const ZapretSettings& settings)
{
    detail::Json node = detail::Json::object();
    if (!settings.installDirectory.empty())
    {
        node["installDirectory"] = settings.installDirectory;
    }
    if (!settings.winwsPath.empty())
    {
        node["winwsPath"] = settings.winwsPath;
    }
    if (!settings.nfqwsPath.empty())
    {
        node["nfqwsPath"] = settings.nfqwsPath;
    }
    if (!settings.tpwsPath.empty())
    {
        node["tpwsPath"] = settings.tpwsPath;
    }
    if (!settings.workingDirectory.empty())
    {
        node["workingDirectory"] = settings.workingDirectory;
    }
    return node;
}

void readDaemonSettings(const detail::Json& node, DaemonSettings& settings, std::vector<Diagnostic>& diagnostics)
{
    settings.listenAddress = node.value("listenAddress", settings.listenAddress);

    const std::string language = node.value("language", settings.language);
    if (i18n::parseLanguage(language, i18n::Language::system) == i18n::Language::system &&
        !strings::equalsIgnoreCase(strings::trim(language), "auto") &&
        !strings::equalsIgnoreCase(strings::trim(language), "system"))
    {
        diagnostics.push_back(errorAt("config.daemon.language.unknown", "daemon.language", {language}));
    }
    else
    {
        settings.language = language;
    }

    settings.allowRemoteClients = node.value("allowRemoteClients", settings.allowRemoteClients);
    settings.requireAuthentication = node.value("requireAuthentication", settings.requireAuthentication);
    settings.tokenFile = node.value("tokenFile", settings.tokenFile);
    settings.autoStartEngine = node.value("autoStartEngine", settings.autoStartEngine);
    settings.activeProfileId = node.value("activeProfileId", settings.activeProfileId);
    settings.enableTls = node.value("enableTls", settings.enableTls);
    settings.tlsCertificateFile = node.value("tlsCertificateFile", settings.tlsCertificateFile);
    settings.tlsPrivateKeyFile = node.value("tlsPrivateKeyFile", settings.tlsPrivateKeyFile);

    const int port = node.value("port", static_cast<int>(settings.port));
    if (port > 0 && port <= 65535)
    {
        settings.port = static_cast<std::uint16_t>(port);
    }
    else
    {
        diagnostics.push_back(errorAt("config.daemon.port.range", "daemon.port"));
    }

    const std::string levelText = node.value("logLevel", std::string{toString(settings.logLevel)});
    LogLevel level = settings.logLevel;
    if (parseLogLevel(levelText, level))
    {
        settings.logLevel = level;
    }
    else
    {
        diagnostics.push_back(errorAt("config.daemon.log_level.unknown", "daemon.logLevel", {levelText}));
    }

    const std::int64_t historySize = node.value("logHistorySize", static_cast<std::int64_t>(settings.logHistorySize));
    if (historySize >= 100 && historySize <= 100000)
    {
        settings.logHistorySize = static_cast<std::size_t>(historySize);
    }
    else
    {
        diagnostics.push_back(errorAt("config.daemon.log_history.range", "daemon.logHistorySize"));
    }
}

void readEngineSettings(const detail::Json& node, EngineSettings& settings)
{
    settings.maxRestarts = node.value("maxRestarts", settings.maxRestarts);
    settings.restartBackoffMs = node.value("restartBackoffMs", settings.restartBackoffMs);
    settings.stopTimeoutMs = node.value("stopTimeoutMs", settings.stopTimeoutMs);
    settings.restartOnCrash = node.value("restartOnCrash", settings.restartOnCrash);
}

void readZapretSettings(const detail::Json& node, ZapretSettings& settings)
{
    settings.installDirectory = node.value("installDirectory", settings.installDirectory);
    settings.winwsPath = node.value("winwsPath", settings.winwsPath);
    settings.nfqwsPath = node.value("nfqwsPath", settings.nfqwsPath);
    settings.tpwsPath = node.value("tpwsPath", settings.tpwsPath);
    settings.workingDirectory = node.value("workingDirectory", settings.workingDirectory);
}

} // namespace

const Profile* DeimosConfig::findProfile(std::string_view id) const
{
    for (const Profile& profile : profiles)
    {
        if (profile.id == id)
        {
            return &profile;
        }
    }
    return nullptr;
}

Profile* DeimosConfig::findProfile(std::string_view id)
{
    for (Profile& profile : profiles)
    {
        if (profile.id == id)
        {
            return &profile;
        }
    }
    return nullptr;
}

bool DeimosConfig::upsertProfile(Profile profile)
{
    for (Profile& existing : profiles)
    {
        if (existing.id == profile.id)
        {
            existing = std::move(profile);
            return true;
        }
    }
    profiles.push_back(std::move(profile));
    return false;
}

bool DeimosConfig::removeProfile(std::string_view id)
{
    const auto iterator = std::remove_if(profiles.begin(),
                                         profiles.end(),
                                         [id](const Profile& profile)
                                         {
                                             return profile.id == id;
                                         });
    if (iterator == profiles.end())
    {
        return false;
    }
    profiles.erase(iterator, profiles.end());
    return true;
}

DeimosConfig defaultConfig()
{
    DeimosConfig config;
    config.schemaVersion = 1;
    config.daemon.activeProfileId = "tls-split";
    config.profiles.push_back(makeTlsSplitProfile());
    config.profiles.push_back(makeQuicSplitProfile());
    return config;
}

std::string serializeConfig(const DeimosConfig& config, bool pretty)
{
    detail::Json root{
        {"schemaVersion", config.schemaVersion},
        {"daemon", writeDaemonSettings(config.daemon)},
        {"engine", writeEngineSettings(config.engine)},
        {"zapret", writeZapretSettings(config.zapret)},
    };

    detail::Json profiles = detail::Json::array();
    for (const Profile& profile : config.profiles)
    {
        profiles.push_back(detail::writeProfileJson(profile));
    }
    root["profiles"] = std::move(profiles);
    return root.dump(pretty ? 2 : -1);
}

Status deserializeConfig(std::string_view text, DeimosConfig& out, std::vector<Diagnostic>& diagnostics)
{
    detail::Json root;
    try
    {
        root = detail::Json::parse(text);
    }
    catch (const std::exception& exception)
    {
        diagnostics.push_back(errorAt("config.parse_error", "", {exception.what()}));
        return Status::failure(ErrorCode::configuration, i18n::tr("err.config.invalid_json"), exception.what());
    }

    if (!root.is_object())
    {
        diagnostics.push_back(errorAt("config.invalid_root", ""));
        return Status::failure(ErrorCode::configuration, i18n::tr("err.config.not_an_object"));
    }

    DeimosConfig config = defaultConfig();
    config.profiles.clear();

    try
    {
        const int schemaVersion = root.value("schemaVersion", 1);
        if (schemaVersion > 1)
        {
            diagnostics.push_back(
                errorAt("config.schema.too_new", "schemaVersion", {std::to_string(schemaVersion)}));
            return Status::failure(ErrorCode::notSupported,
                                   i18n::tr("err.config.schema"),
                                   std::to_string(schemaVersion));
        }
        config.schemaVersion = schemaVersion;

        if (const auto daemonNode = root.find("daemon"); daemonNode != root.end() && daemonNode->is_object())
        {
            readDaemonSettings(*daemonNode, config.daemon, diagnostics);
        }
        if (const auto engineNode = root.find("engine"); engineNode != root.end() && engineNode->is_object())
        {
            readEngineSettings(*engineNode, config.engine);
        }
        if (const auto zapretNode = root.find("zapret"); zapretNode != root.end() && zapretNode->is_object())
        {
            readZapretSettings(*zapretNode, config.zapret);
        }

        if (const auto profilesNode = root.find("profiles"); profilesNode != root.end())
        {
            if (!profilesNode->is_array())
            {
                diagnostics.push_back(errorAt("config.profiles.invalid", "profiles"));
            }
            else
            {
                std::size_t index = 0;
                for (const detail::Json& profileNode : *profilesNode)
                {
                    const std::string path = "profiles[" + std::to_string(index) + "]";
                    auto profile = detail::parseProfileJson(profileNode, path, diagnostics);
                    if (profile.id.empty())
                    {
                        diagnostics.push_back(errorAt("profile.id.empty", path + ".id"));
                    }
                    config.profiles.push_back(std::move(profile));
                    ++index;
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        diagnostics.push_back(errorAt("config.invalid_type", "", {exception.what()}));
        return Status::failure(ErrorCode::configuration, i18n::tr("err.config.wrong_type"), exception.what());
    }

    const auto validationDiagnostics = validateConfig(config);
    diagnostics.insert(diagnostics.end(), validationDiagnostics.begin(), validationDiagnostics.end());

    out = std::move(config);
    if (hasErrors(diagnostics))
    {
        return Status::failure(ErrorCode::configuration,
                               i18n::tr("err.config.invalid"),
                               formatDiagnostics(diagnostics));
    }
    return Status::ok();
}

std::vector<Diagnostic> validateConfig(const DeimosConfig& config)
{
    std::vector<Diagnostic> diagnostics;

    for (std::size_t index = 0; index < config.profiles.size(); ++index)
    {
        const auto profileDiagnostics = validateProfile(config.profiles[index], index);
        diagnostics.insert(diagnostics.end(), profileDiagnostics.begin(), profileDiagnostics.end());

        for (std::size_t other = 0; other < index; ++other)
        {
            if (config.profiles[other].id == config.profiles[index].id)
            {
                diagnostics.push_back(errorAt("config.profiles.duplicate_id",
                                              "profiles[" + std::to_string(index) + "].id",
                                              {config.profiles[index].id}));
            }
        }
    }

    const bool isLoopback = config.daemon.listenAddress == "127.0.0.1" || config.daemon.listenAddress == "localhost" ||
                            config.daemon.listenAddress == "::1";
    if (!isLoopback && !config.daemon.allowRemoteClients)
    {
        diagnostics.push_back(errorAt("config.daemon.remote_not_allowed",
                                      "daemon.listenAddress",
                                      {config.daemon.listenAddress}));
    }
    if (!isLoopback && !config.daemon.requireAuthentication)
    {
        diagnostics.push_back(errorAt("config.daemon.remote_without_auth", "daemon.requireAuthentication"));
    }

    if (config.daemon.enableTls && (config.daemon.tlsCertificateFile.empty() || config.daemon.tlsPrivateKeyFile.empty()))
    {
        diagnostics.push_back(errorAt("config.daemon.tls.incomplete", "daemon.enableTls"));
    }

    if (!config.daemon.activeProfileId.empty() && config.findProfile(config.daemon.activeProfileId) == nullptr)
    {
        diagnostics.push_back(errorAt("config.daemon.active_profile.missing",
                                      "daemon.activeProfileId",
                                      {config.daemon.activeProfileId}));
    }
    else if (const Profile* active = config.findProfile(config.daemon.activeProfileId);
             active != nullptr && !active->enabled)
    {
        diagnostics.push_back(warningAt("config.daemon.active_profile.disabled",
                                        "daemon.activeProfileId",
                                        {active->id}));
    }

    if (config.engine.maxRestarts < 0 || config.engine.maxRestarts > 100)
    {
        diagnostics.push_back(errorAt("config.engine.max_restarts.range", "engine.maxRestarts"));
    }
    if (config.engine.restartBackoffMs < 0 || config.engine.restartBackoffMs > 600000)
    {
        diagnostics.push_back(errorAt("config.engine.backoff.range", "engine.restartBackoffMs"));
    }
    if (config.engine.stopTimeoutMs < 100 || config.engine.stopTimeoutMs > 120000)
    {
        diagnostics.push_back(errorAt("config.engine.stop_timeout.range", "engine.stopTimeoutMs"));
    }

    return diagnostics;
}

std::string defaultConfigPath()
{
    return (platform::configDirectory() / "deimos.json").string();
}

std::string defaultTokenPath()
{
    return (platform::stateDirectory() / "token").string();
}

}
