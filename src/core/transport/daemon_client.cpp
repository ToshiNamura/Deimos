#include <deimos/transport/daemon_client.hpp>

#include <exception>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

std::string apiPath(std::string_view suffix)
{
    return std::string{API_PREFIX} + "/" + std::string{suffix};
}

std::string describeTransportError(httplib::Error error)
{
    switch (error)
    {
    case httplib::Error::Connection:
        return "connection refused or unreachable";
    case httplib::Error::ConnectionTimeout:
        return "connection timed out";
    case httplib::Error::Timeout:
        return "request timed out";
    case httplib::Error::Read:
        return "connection closed while reading the response";
    case httplib::Error::Write:
        return "connection closed while writing the request";
    case httplib::Error::Canceled:
        return "request canceled";
    default:
        break;
    }
    return "network error";
}

std::uint16_t defaultPortForScheme(const std::string& scheme)
{
    return scheme == "https" ? 443 : 8787;
}

} // namespace

std::string Endpoint::describe() const
{
    std::string text = scheme + "://" + host + ":" + std::to_string(port);
    if (!basePath.empty())
    {
        text += basePath;
    }
    return text;
}

bool parseEndpoint(std::string_view text, Endpoint& out, std::string& errorMessage)
{
    std::string value = strings::trim(text);
    if (value.empty())
    {
        errorMessage = "endpoint is empty";
        return false;
    }

    Endpoint endpoint;

    const std::size_t schemeSeparator = value.find("://");
    if (schemeSeparator != std::string::npos)
    {
        endpoint.scheme = strings::toLower(value.substr(0, schemeSeparator));
        value = value.substr(schemeSeparator + 3);
    }
    if (endpoint.scheme != "http" && endpoint.scheme != "https")
    {
        errorMessage = "unsupported scheme '" + endpoint.scheme + "'";
        return false;
    }

    const std::size_t slash = value.find('/');
    if (slash != std::string::npos)
    {
        endpoint.basePath = value.substr(slash);
        while (!endpoint.basePath.empty() && endpoint.basePath.back() == '/')
        {
            endpoint.basePath.pop_back();
        }
        value = value.substr(0, slash);
    }

    const std::size_t colon = value.rfind(':');
    if (colon != std::string::npos)
    {
        std::int64_t port = 0;
        if (!strings::parseInt(value.substr(colon + 1), port) || port <= 0 || port > 65535)
        {
            errorMessage = "invalid port in endpoint '" + value + "'";
            return false;
        }
        endpoint.port = static_cast<std::uint16_t>(port);
        endpoint.host = value.substr(0, colon);
    }
    else
    {
        endpoint.host = value;
        endpoint.port = defaultPortForScheme(endpoint.scheme);
    }

    if (endpoint.host.empty())
    {
        errorMessage = "endpoint has no host";
        return false;
    }

    out = endpoint;
    return true;
}

DaemonClient::DaemonClient(ClientOptions options)
    : options_(std::move(options))
{
}

const Endpoint& DaemonClient::endpoint() const
{
    return options_.endpoint;
}

void DaemonClient::setToken(std::string token)
{
    options_.token = std::move(token);
}

bool DaemonClient::hasToken() const
{
    return !options_.token.empty();
}

Result<std::string> DaemonClient::call(std::string_view method,
                                       std::string_view path,
                                       std::string_view body,
                                       bool authenticated) const
{
    const Endpoint& endpoint = options_.endpoint;
    if (endpoint.scheme == "https")
    {
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
        return Result<std::string>{Error{ErrorCode::notSupported,
                                         "HTTPS is not available in this build",
                                         "rebuild with -DDEIMOS_ENABLE_TLS=ON"}};
#endif
    }

    httplib::Client client(endpoint.host, static_cast<int>(endpoint.port));
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(options_.timeoutSeconds, 0);
    client.set_write_timeout(options_.timeoutSeconds, 0);
    client.set_keep_alive(false);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    client.enable_server_certificate_verification(true);
#endif

    httplib::Headers headers;
    if (authenticated && !options_.token.empty())
    {
        headers.emplace("Authorization", "Bearer " + options_.token);
    }
    headers.emplace("Accept", "application/json");

    const std::string target = endpoint.basePath + std::string{path};
    const std::string requestBody{body};

    httplib::Result response;
    const std::string upperMethod = strings::toUpper(method);
    if (upperMethod == "GET")
    {
        response = client.Get(target.c_str(), headers);
    }
    else if (upperMethod == "POST")
    {
        response = client.Post(target.c_str(), headers, requestBody, "application/json");
    }
    else if (upperMethod == "PUT")
    {
        response = client.Put(target.c_str(), headers, requestBody, "application/json");
    }
    else if (upperMethod == "DELETE")
    {
        response = requestBody.empty() ? client.Delete(target.c_str(), headers)
                                       : client.Delete(target.c_str(), headers, requestBody, "application/json");
    }
    else
    {
        return Result<std::string>{Error{ErrorCode::invalidArgument, "unsupported HTTP method", upperMethod}};
    }

    if (!response)
    {
        return Result<std::string>{Error{ErrorCode::network,
                                         "cannot reach the daemon at " + endpoint.describe(),
                                         describeTransportError(response.error())}};
    }

    const int status = response->status;
    if (status >= 200 && status < 300)
    {
        return Result<std::string>{std::move(response->body)};
    }

    Error error;
    if (parseErrorPayload(response->body, error).isOk())
    {
        return Result<std::string>{error};
    }

    return Result<std::string>{
        Error{status == 401 ? ErrorCode::unauthorized : ErrorCode::internal,
              "daemon returned HTTP " + std::to_string(status),
              strings::trim(response->body)}};
}

Result<HealthInfo> DaemonClient::health() const
{
    const auto response = call("GET", apiPath("health"), {}, false);
    if (!response)
    {
        return Result<HealthInfo>{response.error()};
    }
    HealthInfo info;
    if (const auto status = parseHealth(response.value(), info); !status.isOk())
    {
        return Result<HealthInfo>{status.error()};
    }
    return Result<HealthInfo>{std::move(info)};
}

Result<DaemonInfo> DaemonClient::status() const
{
    const auto response = call("GET", apiPath("status"));
    if (!response)
    {
        return Result<DaemonInfo>{response.error()};
    }
    DaemonInfo info;
    if (const auto status = parseDaemonInfo(response.value(), info); !status.isOk())
    {
        return Result<DaemonInfo>{status.error()};
    }
    return Result<DaemonInfo>{std::move(info)};
}

Result<EngineSnapshot> DaemonClient::startEngine(std::string_view profileId) const
{
    const std::string body = profileId.empty() ? std::string{"{}"}
                                               : std::string{"{\"profileId\":\""} + std::string{profileId} + "\"}";
    const auto response = call("POST", apiPath("engine/start"), body);
    if (!response)
    {
        return Result<EngineSnapshot>{response.error()};
    }
    EngineSnapshot snapshot;
    if (const auto status = parseEngineSnapshot(response.value(), snapshot); !status.isOk())
    {
        return Result<EngineSnapshot>{status.error()};
    }
    return Result<EngineSnapshot>{std::move(snapshot)};
}

Result<EngineSnapshot> DaemonClient::stopEngine() const
{
    const auto response = call("POST", apiPath("engine/stop"), "{}");
    if (!response)
    {
        return Result<EngineSnapshot>{response.error()};
    }
    EngineSnapshot snapshot;
    if (const auto status = parseEngineSnapshot(response.value(), snapshot); !status.isOk())
    {
        return Result<EngineSnapshot>{status.error()};
    }
    return Result<EngineSnapshot>{std::move(snapshot)};
}

Result<EngineSnapshot> DaemonClient::restartEngine(std::string_view profileId) const
{
    const std::string body = profileId.empty() ? std::string{"{}"}
                                               : std::string{"{\"profileId\":\""} + std::string{profileId} + "\"}";
    const auto response = call("POST", apiPath("engine/restart"), body);
    if (!response)
    {
        return Result<EngineSnapshot>{response.error()};
    }
    EngineSnapshot snapshot;
    if (const auto status = parseEngineSnapshot(response.value(), snapshot); !status.isOk())
    {
        return Result<EngineSnapshot>{status.error()};
    }
    return Result<EngineSnapshot>{std::move(snapshot)};
}

Result<std::vector<ProfileSummary>> DaemonClient::listProfiles() const
{
    const auto response = call("GET", apiPath("profiles"));
    if (!response)
    {
        return Result<std::vector<ProfileSummary>>{response.error()};
    }

    std::vector<ProfileSummary> summaries;
    try
    {
        const auto root = nlohmann::json::parse(response.value());
        const auto profiles = root.find("profiles");
        if (profiles != root.end() && profiles->is_array())
        {
            for (const auto& item : *profiles)
            {
                ProfileSummary summary;
                summary.id = item.value("id", std::string{});
                summary.name = item.value("name", std::string{});
                summary.description = item.value("description", std::string{});
                summary.enabled = item.value("enabled", true);
                summary.roundCount = item.value("rounds", static_cast<std::size_t>(0));
                ToolKind tool = ToolKind::autoDetect;
                parseToolKind(item.value("tool", std::string{"auto"}), tool);
                summary.tool = tool;
                summaries.push_back(std::move(summary));
            }
        }
    }
    catch (const std::exception& exception)
    {
        return Result<std::vector<ProfileSummary>>{
            Error{ErrorCode::invalidArgument, "cannot parse profile list", exception.what()}};
    }
    return Result<std::vector<ProfileSummary>>{std::move(summaries)};
}

Result<Profile> DaemonClient::getProfile(std::string_view id) const
{
    const auto response = call("GET", apiPath("profiles/" + std::string{id}));
    if (!response)
    {
        return Result<Profile>{response.error()};
    }
    Profile profile;
    std::vector<Diagnostic> diagnostics;
    if (const auto status = parseProfile(response.value(), profile, diagnostics); !status.isOk())
    {
        return Result<Profile>{Error{ErrorCode::invalidArgument, "cannot parse profile", status.error().describe()}};
    }
    return Result<Profile>{std::move(profile)};
}

Result<Profile> DaemonClient::saveProfile(const Profile& profile) const
{
    const auto response = call("PUT", apiPath("profiles/" + profile.id), serializeProfile(profile, false));
    if (!response)
    {
        return Result<Profile>{response.error()};
    }
    Profile saved;
    std::vector<Diagnostic> diagnostics;
    if (const auto status = parseProfile(response.value(), saved, diagnostics); !status.isOk())
    {
        return Result<Profile>{
            Error{ErrorCode::invalidArgument, "cannot parse saved profile", status.error().describe()}};
    }
    return Result<Profile>{std::move(saved)};
}

Status DaemonClient::deleteProfile(std::string_view id) const
{
    const auto response = call("DELETE", apiPath("profiles/" + std::string{id}));
    if (!response)
    {
        return Status{response.error()};
    }
    return Status::ok();
}

Result<CommandPreview> DaemonClient::previewProfile(std::string_view id) const
{
    const auto response = call("GET", apiPath("profiles/" + std::string{id} + "/preview"));
    if (!response)
    {
        return Result<CommandPreview>{response.error()};
    }
    CommandPreview preview;
    if (const auto status = parseCommandPreview(response.value(), preview); !status.isOk())
    {
        return Result<CommandPreview>{status.error()};
    }
    return Result<CommandPreview>{std::move(preview)};
}

Result<EngineSnapshot> DaemonClient::activateProfile(std::string_view id) const
{
    const auto response = call("POST", apiPath("profiles/" + std::string{id} + "/activate"), "{}");
    if (!response)
    {
        return Result<EngineSnapshot>{response.error()};
    }
    EngineSnapshot snapshot;
    if (const auto status = parseEngineSnapshot(response.value(), snapshot); !status.isOk())
    {
        return Result<EngineSnapshot>{status.error()};
    }
    return Result<EngineSnapshot>{std::move(snapshot)};
}

Result<DeimosConfig> DaemonClient::getConfig() const
{
    const auto response = call("GET", apiPath("config"));
    if (!response)
    {
        return Result<DeimosConfig>{response.error()};
    }
    DeimosConfig config;
    std::vector<Diagnostic> diagnostics;
    if (const auto status = deserializeConfig(response.value(), config, diagnostics); !status.isOk())
    {
        return Result<DeimosConfig>{status.error()};
    }
    return Result<DeimosConfig>{std::move(config)};
}

Status DaemonClient::putConfig(const DeimosConfig& config) const
{
    const auto response = call("PUT", apiPath("config"), serializeConfig(config, false));
    if (!response)
    {
        return Status{response.error()};
    }
    return Status::ok();
}

Result<std::vector<LogEntry>> DaemonClient::logs(std::size_t limit,
                                                 LogLevel minimum,
                                                 std::uint64_t afterSequence) const
{
    std::string path = apiPath("logs");
    path += "?limit=" + std::to_string(limit);
    path += "&level=";
    path += std::string{toString(minimum)};
    if (afterSequence != 0)
    {
        path += "&after=" + std::to_string(afterSequence);
    }

    const auto response = call("GET", path);
    if (!response)
    {
        return Result<std::vector<LogEntry>>{response.error()};
    }

    std::vector<LogEntry> entries;
    std::uint64_t lastSequence = 0;
    if (const auto status = parseLogEntries(response.value(), entries, lastSequence); !status.isOk())
    {
        return Result<std::vector<LogEntry>>{status.error()};
    }
    return Result<std::vector<LogEntry>>{std::move(entries)};
}

Result<ZapretInfo> DaemonClient::zapret() const
{
    const auto response = call("GET", apiPath("zapret"));
    if (!response)
    {
        return Result<ZapretInfo>{response.error()};
    }
    ZapretInfo info;
    if (const auto status = parseZapretInfo(response.value(), info); !status.isOk())
    {
        return Result<ZapretInfo>{status.error()};
    }
    return Result<ZapretInfo>{std::move(info)};
}

}
