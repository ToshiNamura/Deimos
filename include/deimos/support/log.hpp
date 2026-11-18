#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace deimos
{

enum class LogLevel : std::uint8_t
{
    trace = 0,
    debug,
    info,
    warning,
    error,
};

std::string_view toString(LogLevel level);

/// Parses "trace", "debug", "info", "warn"/"warning", "error" (case insensitive).
bool parseLogLevel(std::string_view text, LogLevel& out);

struct LogEntry
{
    std::uint64_t sequence = 0;
    std::chrono::system_clock::time_point timestamp;
    LogLevel level = LogLevel::info;
    std::string source;
    std::string message;
};

class LogSink
{
public:
    virtual ~LogSink() = default;

    virtual void write(const LogEntry& entry) = 0;
};

/// Bounded, thread-safe in-memory history served by the API (GET /logs).
class LogRingBuffer final : public LogSink
{
public:
    explicit LogRingBuffer(std::size_t capacity = 2000);

    void write(const LogEntry& entry) override;

    /// Most recent entries, oldest first.
    std::vector<LogEntry> snapshot(std::size_t limit, LogLevel minimum = LogLevel::trace) const;

    /// Entries newer than \p afterSequence, oldest first.
    std::vector<LogEntry> snapshotAfter(std::uint64_t afterSequence,
                                        std::size_t limit,
                                        LogLevel minimum = LogLevel::trace) const;

    std::uint64_t lastSequence() const;

    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
    std::size_t capacity_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};

/// Writes human readable lines to stdout/stderr.
class ConsoleSink final : public LogSink
{
public:
    void write(const LogEntry& entry) override;
};

/// Appends lines to a file, re-opening it on rotation or external truncation.
class FileSink final : public LogSink
{
public:
    explicit FileSink(std::string path);
    ~FileSink() override;

    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    void write(const LogEntry& entry) override;

    const std::string& path() const noexcept
    {
        return path_;
    }

private:
    void reopen();

    std::string path_;
    std::mutex mutex_;
    void* handle_ = nullptr;
};

class Logger
{
public:
    static Logger& global();

    void addSink(std::shared_ptr<LogSink> sink);
    void clearSinks();

    void setLevel(LogLevel level);
    LogLevel level() const;

    bool isEnabled(LogLevel level) const;

    void log(LogLevel level, std::string_view source, std::string message);

    void trace(std::string_view source, std::string message);
    void debug(std::string_view source, std::string message);
    void info(std::string_view source, std::string message);
    void warning(std::string_view source, std::string message);
    void error(std::string_view source, std::string message);

private:
    Logger() = default;

    std::atomic<LogLevel> level_{LogLevel::info};
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::uint64_t nextSequence_ = 1;
};

/// Convenience helpers that write through Logger::global().
namespace log
{

void trace(std::string_view source, std::string message);
void debug(std::string_view source, std::string message);
void info(std::string_view source, std::string message);
void warning(std::string_view source, std::string message);
void error(std::string_view source, std::string message);

} // namespace log

/// "2026-11-18T17:48:42.123Z" in UTC.
std::string formatTimestamp(const std::chrono::system_clock::time_point& timePoint);

/// Parses a timestamp produced by formatTimestamp.
bool parseTimestamp(std::string_view text, std::chrono::system_clock::time_point& out);

}
