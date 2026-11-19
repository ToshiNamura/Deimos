#include <deimos/transport/http_server.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

#include <httplib.h>

#include <deimos/support/i18n.hpp>
#include <deimos/support/log.hpp>
#include <deimos/support/strings.hpp>
#include <deimos/transport/protocol.hpp>

namespace deimos
{
namespace
{

std::string urlDecode(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    const auto hexValue = [](char value) -> int
    {
        if (value >= '0' && value <= '9')
        {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f')
        {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F')
        {
            return value - 'A' + 10;
        }
        return -1;
    };

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const char character = text[index];
        if (character == '+')
        {
            result.push_back(' ');
            continue;
        }
        if (character == '%' && index + 2 < text.size())
        {
            const int high = hexValue(text[index + 1]);
            const int low = hexValue(text[index + 2]);
            if (high >= 0 && low >= 0)
            {
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        result.push_back(character);
    }
    return result;
}

std::map<std::string, std::string> parseQueryString(std::string_view target)
{
    std::map<std::string, std::string> query;
    const std::size_t questionMark = target.find('?');
    if (questionMark == std::string_view::npos)
    {
        return query;
    }

    for (const std::string& pair : strings::split(target.substr(questionMark + 1), '&'))
    {
        if (pair.empty())
        {
            continue;
        }
        const std::size_t equals = pair.find('=');
        if (equals == std::string::npos)
        {
            query[urlDecode(pair)] = std::string{};
            continue;
        }
        query[urlDecode(std::string_view{pair}.substr(0, equals))] = urlDecode(std::string_view{pair}.substr(equals + 1));
    }
    return query;
}

std::vector<std::string> splitPath(std::string_view path)
{
    std::vector<std::string> segments;
    for (const std::string& part : strings::split(path, '/'))
    {
        if (!part.empty())
        {
            segments.push_back(part);
        }
    }
    return segments;
}

struct RouteEntry
{
    std::string method;
    std::vector<std::string> segments;
    HttpHandler handler;
};

} // namespace

std::string HttpRequest::header(std::string_view name) const
{
    const auto iterator = headers.find(strings::toLower(name));
    if (iterator == headers.end())
    {
        return {};
    }
    return iterator->second;
}

std::string HttpRequest::queryValue(std::string_view name, std::string defaultValue) const
{
    const auto iterator = query.find(std::string{name});
    if (iterator == query.end())
    {
        return defaultValue;
    }
    return iterator->second;
}

struct HttpServer::Impl
{
    httplib::Server server;
    std::thread worker;
    std::atomic<bool> running{false};
    std::uint16_t boundPort = 0;
    std::string lastError;
    std::vector<RouteEntry> routes;
    HttpFilter filter;
    std::mutex mutex;
    bool configured = false;

    HttpResponse dispatch(const HttpRequest& request)
    {
        if (filter)
        {
            if (auto shortCircuit = filter(request); shortCircuit.has_value())
            {
                return *shortCircuit;
            }
        }

        const std::vector<std::string> requestSegments = splitPath(request.path);
        std::lock_guard<std::mutex> lock(mutex);
        for (const RouteEntry& entry : routes)
        {
            if (entry.method != request.method && entry.method != "*")
            {
                continue;
            }
            if (entry.segments.size() != requestSegments.size())
            {
                continue;
            }

            std::map<std::string, std::string> parameters;
            bool matched = true;
            for (std::size_t index = 0; index < entry.segments.size(); ++index)
            {
                const std::string& patternSegment = entry.segments[index];
                if (!patternSegment.empty() && patternSegment.front() == ':')
                {
                    parameters[patternSegment.substr(1)] = requestSegments[index];
                    continue;
                }
                if (patternSegment != requestSegments[index])
                {
                    matched = false;
                    break;
                }
            }
            if (!matched)
            {
                continue;
            }

            HttpRequest enriched = request;
            enriched.pathParams = std::move(parameters);
            return entry.handler(enriched);
        }

        HttpResponse notFound;
        notFound.status = 404;
        notFound.body = serializeErrorPayload(Error{ErrorCode::notFound, std::string{i18n::tr("api.error.unknownEndpoint")}});
        return notFound;
    }

    void handle(const char* method, const httplib::Request& request, httplib::Response& response)
    {
        HttpRequest converted;
        converted.method = method;
        converted.path = request.path;
        converted.body = request.body;
        converted.query = parseQueryString(request.target);
        for (const auto& entry : request.headers)
        {
            converted.headers[strings::toLower(entry.first)] = entry.second;
        }

        HttpResponse result;
        try
        {
            result = dispatch(converted);
        }
        catch (const std::exception& exception)
        {
            log::error("http",
                       i18n::tr("log.http.unhandled", {converted.path, exception.what()}));
            result.status = 500;
            result.body =
                serializeErrorPayload(Error{ErrorCode::internal, std::string{i18n::tr("api.error.unhandled")}});
        }

        response.status = result.status;
        for (const auto& header : result.headers)
        {
            response.set_header(header.first, header.second);
        }
        response.set_content(result.body, result.contentType.c_str());
    }

    void configure()
    {
        if (configured)
        {
            return;
        }
        configured = true;

        server.set_payload_max_length(1024 * 1024);
        server.set_read_timeout(15, 0);
        server.set_write_timeout(15, 0);
        server.set_exception_handler(
            [](const httplib::Request&, httplib::Response& response, std::exception_ptr pointer)
            {
                std::string detail;
                if (pointer)
                {
                    try
                    {
                        std::rethrow_exception(pointer);
                    }
                    catch (const std::exception& exception)
                    {
                        detail = exception.what();
                    }
                }
                response.status = 500;
                response.set_content(R"({"error":{"code":"internal_error","message":"request failed","detail":")" +
                                         detail + R"("}})",
                                     "application/json");
            });
        server.set_logger(
            [](const httplib::Request& request, const httplib::Response& response)
            {
                log::debug("http", request.method + " " + request.path + " -> " + std::to_string(response.status));
            });

        server.Get(R"(.*)", [this](const httplib::Request& request, httplib::Response& response)
        {
            handle("GET", request, response);
        });
        server.Post(R"(.*)", [this](const httplib::Request& request, httplib::Response& response)
        {
            handle("POST", request, response);
        });
        server.Put(R"(.*)", [this](const httplib::Request& request, httplib::Response& response)
        {
            handle("PUT", request, response);
        });
        server.Delete(R"(.*)", [this](const httplib::Request& request, httplib::Response& response)
        {
            handle("DELETE", request, response);
        });
        server.Options(R"(.*)", [this](const httplib::Request& request, httplib::Response& response)
        {
            handle("OPTIONS", request, response);
        });
    }

    void beginServing(std::uint16_t port)
    {
        boundPort = port;
        running.store(true);
        worker = std::thread(
            [this]
            {
                server.listen_after_bind();
                running.store(false);
            });
    }
};

HttpServer::HttpServer()
    : impl_(std::make_unique<Impl>())
{
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::route(std::string method, std::string path, HttpHandler handler)
{
    RouteEntry entry;
    entry.method = std::move(method);
    entry.segments = splitPath(path);
    entry.handler = std::move(handler);

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->routes.push_back(std::move(entry));
}

void HttpServer::routePattern(std::string method, std::string pattern, HttpHandler handler)
{
    route(std::move(method), std::move(pattern), std::move(handler));
}

void HttpServer::setFilter(HttpFilter filter)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->filter = std::move(filter);
}

Status HttpServer::start(std::string_view address, std::uint16_t port)
{
    if (impl_->running.load())
    {
        return Status::failure(ErrorCode::conflict, i18n::tr("err.http.alreadyRunning"));
    }

    impl_->configure();
    const std::string host{address};
    if (!impl_->server.bind_to_port(host, static_cast<int>(port)))
    {
        impl_->lastError = "cannot bind to " + host + ":" + std::to_string(port);
        return Status::failure(ErrorCode::io, impl_->lastError);
    }

    impl_->beginServing(port);
    return Status::ok();
}

Status HttpServer::startEphemeral(std::string_view address)
{
    if (impl_->running.load())
    {
        return Status::failure(ErrorCode::conflict, i18n::tr("err.http.alreadyRunning"));
    }

    impl_->configure();
    const std::string host{address};
    const int bound = impl_->server.bind_to_any_port(host);
    if (bound <= 0)
    {
        impl_->lastError = "cannot bind an ephemeral port on " + host;
        return Status::failure(ErrorCode::io, impl_->lastError);
    }

    impl_->beginServing(static_cast<std::uint16_t>(bound));
    return Status::ok();
}

void HttpServer::stop()
{
    if (!impl_->running.load() && !impl_->worker.joinable())
    {
        return;
    }
    impl_->server.stop();
    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }
    impl_->running.store(false);
}

bool HttpServer::isRunning() const
{
    return impl_->running.load();
}

std::uint16_t HttpServer::boundPort() const
{
    return impl_->boundPort;
}

std::string HttpServer::lastError() const
{
    return impl_->lastError;
}

}
