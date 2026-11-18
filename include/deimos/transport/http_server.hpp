#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/support/error.hpp>

namespace deimos
{

struct HttpRequest
{
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;
    std::string body;
    /// Values captured by ':name' segments of a route pattern.
    std::map<std::string, std::string> pathParams;

    std::string header(std::string_view name) const;
    std::string queryValue(std::string_view name, std::string defaultValue = {}) const;
};

struct HttpResponse
{
    int status = 200;
    std::string contentType{"application/json"};
    std::string body;
    std::map<std::string, std::string> headers;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/// Runs before routing and may short circuit with a response (used for authentication).
using HttpFilter = std::function<std::optional<HttpResponse>(const HttpRequest&)>;

/// Small routing HTTP server that hides the underlying implementation.
class HttpServer
{
public:
    HttpServer();
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    /// Exact path match, for example "/api/v1/health".
    void route(std::string method, std::string path, HttpHandler handler);

    /// Pattern match with ':name' placeholders, for example "/api/v1/profiles/:id".
    void routePattern(std::string method, std::string pattern, HttpHandler handler);

    void setFilter(HttpFilter filter);

    /// Binds and starts serving in a background thread.
    Status start(std::string_view address, std::uint16_t port);

    /// Binds to an ephemeral port; useful in tests.
    Status startEphemeral(std::string_view address);

    void stop();

    bool isRunning() const;
    std::uint16_t boundPort() const;
    std::string lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
