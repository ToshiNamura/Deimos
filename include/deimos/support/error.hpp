#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace deimos
{

enum class ErrorCode : std::uint8_t
{
    none = 0,
    invalidArgument,
    notFound,
    alreadyExists,
    notSupported,
    configuration,
    io,
    network,
    unauthorized,
    conflict,
    unavailable,
    internal,
};

/// Human readable, stable identifier used in API responses and logs.
std::string_view toString(ErrorCode code);

/// Maps an error code onto the HTTP status the daemon should answer with.
int httpStatusFor(ErrorCode code);

class Error
{
public:
    Error() = default;

    Error(ErrorCode code, std::string message, std::string detail = {})
        : code_(code)
        , message_(std::move(message))
        , detail_(std::move(detail))
    {
    }

    ErrorCode code() const noexcept
    {
        return code_;
    }

    const std::string& message() const noexcept
    {
        return message_;
    }

    const std::string& detail() const noexcept
    {
        return detail_;
    }

    /// Composes "message: detail" or just "message" when no detail is present.
    std::string describe() const;

private:
    // A default constructed Error must be "no error" so that Status{} is success.
    ErrorCode code_ = ErrorCode::none;
    std::string message_;
    std::string detail_;
};

/// Result of an operation that produces no value.
class Status
{
public:
    Status() = default;

    Status(Error error)
        : error_(std::move(error))
    {
    }

    static Status ok()
    {
        return Status{};
    }

    static Status failure(ErrorCode code, std::string message, std::string detail = {})
    {
        return Status{Error{code, std::move(message), std::move(detail)}};
    }

    bool isOk() const noexcept
    {
        return error_.code() == ErrorCode::none;
    }

    explicit operator bool() const noexcept
    {
        return isOk();
    }

    const Error& error() const noexcept
    {
        return error_;
    }

private:
    Error error_{};
};

template <typename T>
class Result
{
public:
    Result(T value)
        : storage_(std::in_place_index<0>, std::move(value))
    {
    }

    Result(Error error)
        : storage_(std::in_place_index<1>, std::move(error))
    {
    }

    bool hasValue() const noexcept
    {
        return storage_.index() == 0;
    }

    explicit operator bool() const noexcept
    {
        return hasValue();
    }

    T& value() &
    {
        return std::get<0>(storage_);
    }

    const T& value() const&
    {
        return std::get<0>(storage_);
    }

    T&& value() &&
    {
        return std::get<0>(std::move(storage_));
    }

    const Error& error() const&
    {
        return std::get<1>(storage_);
    }

private:
    std::variant<T, Error> storage_;
};

}
