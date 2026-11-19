#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/diagnostic.hpp>
#include <deimos/model/profile.hpp>
#include <deimos/support/error.hpp>
#include <deimos/support/log.hpp>

namespace deimos
{

struct DaemonSettings
{
    std::string listenAddress = "127.0.0.1";
    std::uint16_t port = 8787;

    /// Language of diagnostics and API messages: "auto", "en" or "ru".
    std::string language{"auto"};

    /// Required when the daemon listens on a non-loopback address.
    bool allowRemoteClients = false;

    /// When true every endpoint except /health requires a bearer token.
    bool requireAuthentication = true;

    /// Overrides the default token location.
    std::string tokenFile;

    LogLevel logLevel = LogLevel::info;
    std::size_t logHistorySize = 2000;

    bool autoStartEngine = false;
    std::string activeProfileId;

    bool enableTls = false;
    std::string tlsCertificateFile;
    std::string tlsPrivateKeyFile;
};

struct EngineSettings
{
    int maxRestarts = 5;
    int restartBackoffMs = 1500;
    int stopTimeoutMs = 5000;
    bool restartOnCrash = true;
};

struct ZapretSettings
{
    /// Root of a zapret installation. Empty means "detect automatically".
    std::string installDirectory;
    /// Explicit tool overrides; empty means "resolve inside installDirectory or PATH".
    std::string winwsPath;
    std::string nfqwsPath;
    std::string tpwsPath;
    /// Working directory for zapret processes, used to resolve relative list paths.
    std::string workingDirectory;
};

struct DeimosConfig
{
    int schemaVersion = 1;
    DaemonSettings daemon;
    EngineSettings engine;
    ZapretSettings zapret;
    std::vector<Profile> profiles;

    const Profile* findProfile(std::string_view id) const;
    Profile* findProfile(std::string_view id);

    /// Adds the profile or replaces the one with the same id. Returns true when replaced.
    bool upsertProfile(Profile profile);
    bool removeProfile(std::string_view id);
};

/// A configuration with sensible defaults and two starter profiles.
DeimosConfig defaultConfig();

/// "{\"schemaVersion\": 1, ...}" ready to be written to disk or sent over HTTP.
std::string serializeConfig(const DeimosConfig& config, bool pretty = true);

/// Parses and validates. Missing fields fall back to defaults; unknown fields are ignored.
Status deserializeConfig(std::string_view text, DeimosConfig& out, std::vector<Diagnostic>& diagnostics);

/// Cross-field validation: uniqueness, active profile existence, port conflicts, TLS files.
std::vector<Diagnostic> validateConfig(const DeimosConfig& config);

/// Default location: <config dir>/deimos.json.
std::string defaultConfigPath();

/// Default token location: <state dir>/token.
std::string defaultTokenPath();

}
