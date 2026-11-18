#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/model/profile.hpp>
#include <deimos/support/error.hpp>
#include <deimos/support/log.hpp>
#include <deimos/transport/protocol.hpp>

namespace deimos
{

/// A parsed daemon address such as "http://127.0.0.1:8787".
struct Endpoint
{
    std::string scheme{"http"};
    std::string host{"127.0.0.1"};
    std::uint16_t port = 8787;
    /// Optional path prefix, for example "/deimos" behind a reverse proxy.
    std::string basePath;

    std::string describe() const;
};

/// Accepts "host", "host:port", "http://host:port" and "https://host:port/base".
bool parseEndpoint(std::string_view text, Endpoint& out, std::string& errorMessage);

struct ClientOptions
{
    Endpoint endpoint;
    std::string token;
    int timeoutSeconds = 10;
};

/// Client for the Deimos daemon API. Shared by the CLI and the desktop UI.
class DaemonClient
{
public:
    explicit DaemonClient(ClientOptions options);

    const Endpoint& endpoint() const;

    /// Replaces the bearer token used for subsequent calls.
    void setToken(std::string token);
    bool hasToken() const;

    /// Cheap liveness probe; works without a token.
    Result<HealthInfo> health() const;

    Result<DaemonInfo> status() const;

    Result<EngineSnapshot> startEngine(std::string_view profileId = {}) const;
    Result<EngineSnapshot> stopEngine() const;
    Result<EngineSnapshot> restartEngine(std::string_view profileId = {}) const;

    Result<std::vector<ProfileSummary>> listProfiles() const;
    Result<Profile> getProfile(std::string_view id) const;
    Result<Profile> saveProfile(const Profile& profile) const;
    Status deleteProfile(std::string_view id) const;
    Result<CommandPreview> previewProfile(std::string_view id) const;
    Result<EngineSnapshot> activateProfile(std::string_view id) const;

    Result<DeimosConfig> getConfig() const;
    Status putConfig(const DeimosConfig& config) const;

    Result<std::vector<LogEntry>> logs(std::size_t limit,
                                       LogLevel minimum = LogLevel::trace,
                                       std::uint64_t afterSequence = 0) const;

    /// Toolchain discovery report produced by the daemon.
    Result<ZapretInfo> zapret() const;

    /// Low level call returning the raw response body.
    Result<std::string> call(std::string_view method,
                             std::string_view path,
                             std::string_view body = {},
                             bool authenticated = true) const;

private:
    ClientOptions options_;
};

}
