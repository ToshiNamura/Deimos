#include <deimos/support/error.hpp>

namespace deimos
{

std::string_view toString(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::none:
        return "ok";
    case ErrorCode::invalidArgument:
        return "invalid_argument";
    case ErrorCode::notFound:
        return "not_found";
    case ErrorCode::alreadyExists:
        return "already_exists";
    case ErrorCode::notSupported:
        return "not_supported";
    case ErrorCode::configuration:
        return "configuration_error";
    case ErrorCode::io:
        return "io_error";
    case ErrorCode::network:
        return "network_error";
    case ErrorCode::unauthorized:
        return "unauthorized";
    case ErrorCode::conflict:
        return "conflict";
    case ErrorCode::unavailable:
        return "unavailable";
    case ErrorCode::internal:
        return "internal_error";
    }
    return "internal_error";
}

int httpStatusFor(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::none:
        return 200;
    case ErrorCode::invalidArgument:
    case ErrorCode::configuration:
        return 400;
    case ErrorCode::unauthorized:
        return 401;
    case ErrorCode::notFound:
        return 404;
    case ErrorCode::alreadyExists:
    case ErrorCode::conflict:
        return 409;
    case ErrorCode::notSupported:
        return 422;
    case ErrorCode::unavailable:
        return 503;
    case ErrorCode::io:
    case ErrorCode::network:
    case ErrorCode::internal:
        return 500;
    }
    return 500;
}

std::string Error::describe() const
{
    if (detail_.empty())
    {
        return message_;
    }
    if (message_.empty())
    {
        return detail_;
    }
    return message_ + ": " + detail_;
}

}
