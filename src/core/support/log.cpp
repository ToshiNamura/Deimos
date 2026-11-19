#include <deimos/support/log.hpp>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include <deimos/support/strings.hpp>

namespace deimos
{

std::string_view toString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::trace:
        return "trace";
    case LogLevel::debug:
        return "debug";
    case LogLevel::info:
        return "info";
    case LogLevel::warning:
        return "warn";
    case LogLevel::error:
        return "error";
    }
    return "info";
}

bool parseLogLevel(std::string_view text, LogLevel& out)
{
    const std::string value = strings::toLower(strings::trim(text));
    if (value == "trace")
    {
        out = LogLevel::trace;
        return true;
    }
    if (value == "debug")
    {
        out = LogLevel::debug;
        return true;
    }
    if (value == "info")
    {
        out = LogLevel::info;
        return true;
    }
    if (value == "warn" || value == "warning")
    {
        out = LogLevel::warning;
        return true;
    }
    if (value == "error")
    {
        out = LogLevel::error;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- LogRingBuffer

LogRingBuffer::LogRingBuffer(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity)
{
    entries_.resize(capacity_);
}

void LogRingBuffer::write(const LogEntry& entry)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ < capacity_)
    {
        entries_[(head_ + size_) % capacity_] = entry;
        ++size_;
        return;
    }
    entries_[head_] = entry;
    head_ = (head_ + 1) % capacity_;
}

std::vector<LogEntry> LogRingBuffer::snapshot(std::size_t limit, LogLevel minimum) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LogEntry> collected;
    collected.reserve(size_);
    for (std::size_t index = 0; index < size_; ++index)
    {
        const LogEntry& entry = entries_[(head_ + index) % capacity_];
        if (entry.level >= minimum)
        {
            collected.push_back(entry);
        }
    }
    if (limit != 0 && collected.size() > limit)
    {
        collected.erase(collected.begin(), collected.end() - static_cast<std::ptrdiff_t>(limit));
    }
    return collected;
}

std::vector<LogEntry> LogRingBuffer::snapshotAfter(std::uint64_t afterSequence,
                                                   std::size_t limit,
                                                   LogLevel minimum) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LogEntry> collected;
    for (std::size_t index = 0; index < size_; ++index)
    {
        const LogEntry& entry = entries_[(head_ + index) % capacity_];
        if (entry.sequence > afterSequence && entry.level >= minimum)
        {
            collected.push_back(entry);
        }
    }
    if (limit != 0 && collected.size() > limit)
    {
        collected.erase(collected.begin(), collected.end() - static_cast<std::ptrdiff_t>(limit));
    }
    return collected;
}

std::uint64_t LogRingBuffer::lastSequence() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0)
    {
        return 0;
    }
    return entries_[(head_ + size_ - 1) % capacity_].sequence;
}

void LogRingBuffer::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    size_ = 0;
}

// ------------------------------------------------------------------ ConsoleSink

void ConsoleSink::write(const LogEntry& entry)
{
    const std::string line = formatTimestamp(entry.timestamp) + " " + strings::toUpper(toString(entry.level)) + " [" +
                             entry.source + "] " + entry.message;

    if (entry.level >= LogLevel::warning)
    {
        std::cerr << line << '\n';
        std::cerr.flush();
        return;
    }
    std::cout << line << '\n';
    std::cout.flush();
}

// --------------------------------------------------------------------- FileSink

FileSink::FileSink(std::string path)
    : path_(std::move(path))
{
    reopen();
}

FileSink::~FileSink()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ != nullptr)
    {
        std::fclose(static_cast<std::FILE*>(handle_));
        handle_ = nullptr;
    }
}

void FileSink::reopen()
{
    if (handle_ != nullptr)
    {
        return;
    }

#ifdef _WIN32
    const std::wstring wide = strings::toWide(path_);
    handle_ = _wfopen(wide.c_str(), L"ab");
#else
    handle_ = std::fopen(path_.c_str(), "ab");
#endif
}

void FileSink::write(const LogEntry& entry)
{
    std::lock_guard<std::mutex> lock(mutex_);
    reopen();
    if (handle_ == nullptr)
    {
        return;
    }
    auto* file = static_cast<std::FILE*>(handle_);
    const std::string line = formatTimestamp(entry.timestamp) + " " + strings::toUpper(toString(entry.level)) + " [" +
                             entry.source + "] " + entry.message + "\n";
    std::fwrite(line.data(), 1, line.size(), file);
    std::fflush(file);
}

// ----------------------------------------------------------------------- Logger

Logger& Logger::global()
{
    static Logger instance;
    return instance;
}

void Logger::addSink(std::shared_ptr<LogSink> sink)
{
    if (!sink)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clearSinks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::setLevel(LogLevel level)
{
    level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::level() const
{
    return level_.load(std::memory_order_relaxed);
}

bool Logger::isEnabled(LogLevel level) const
{
    return level >= this->level();
}

void Logger::log(LogLevel level, std::string_view source, std::string message)
{
    if (!isEnabled(level))
    {
        return;
    }

    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.source = std::string{source};
    entry.message = std::move(message);

    std::lock_guard<std::mutex> lock(mutex_);
    entry.sequence = nextSequence_++;
    for (const auto& sink : sinks_)
    {
        sink->write(entry);
    }
}

void Logger::trace(std::string_view source, std::string message)
{
    log(LogLevel::trace, source, std::move(message));
}

void Logger::debug(std::string_view source, std::string message)
{
    log(LogLevel::debug, source, std::move(message));
}

void Logger::info(std::string_view source, std::string message)
{
    log(LogLevel::info, source, std::move(message));
}

void Logger::warning(std::string_view source, std::string message)
{
    log(LogLevel::warning, source, std::move(message));
}

void Logger::error(std::string_view source, std::string message)
{
    log(LogLevel::error, source, std::move(message));
}

namespace log
{

void trace(std::string_view source, std::string message)
{
    Logger::global().trace(source, std::move(message));
}

void debug(std::string_view source, std::string message)
{
    Logger::global().debug(source, std::move(message));
}

void info(std::string_view source, std::string message)
{
    Logger::global().info(source, std::move(message));
}

void warning(std::string_view source, std::string message)
{
    Logger::global().warning(source, std::move(message));
}

void error(std::string_view source, std::string message)
{
    Logger::global().error(source, std::move(message));
}

} // namespace log

std::string formatTimestamp(const std::chrono::system_clock::time_point& timePoint)
{
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch());
    const std::time_t seconds = std::chrono::system_clock::to_time_t(timePoint);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif

    // Sized for the worst case of every numeric field rendering as a full int.
    char buffer[96];
    // Clamped to the calendar range so the compiler can prove the format below
    // cannot truncate the buffer (-Wformat-truncation under -Werror).
    const auto bounded = [](int value, int minimum, int maximum)
    {
        return value < minimum ? minimum : (value > maximum ? maximum : value);
    };
    const int year = bounded(utc.tm_year + 1900, 0, 9999);
    const int month = bounded(utc.tm_mon + 1, 1, 12);
    const int day = bounded(utc.tm_mday, 1, 31);
    const int hour = bounded(utc.tm_hour, 0, 23);
    const int minute = bounded(utc.tm_min, 0, 59);
    const int second = bounded(utc.tm_sec, 0, 60);
    const int millisecond = bounded(static_cast<int>(millis.count() % 1000), 0, 999);
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  year,
                  month,
                  day,
                  hour,
                  minute,
                  second,
                  millisecond);
    return std::string{buffer};
}

bool parseTimestamp(std::string_view text, std::chrono::system_clock::time_point& out)
{
    const std::string value{text};
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millis = 0;
    if (std::sscanf(value.c_str(),
                    "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ",
                    &year,
                    &month,
                    &day,
                    &hour,
                    &minute,
                    &second,
                    &millis) != 7)
    {
        return false;
    }

    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;

#ifdef _WIN32
    const std::time_t seconds = _mkgmtime(&utc);
#else
    const std::time_t seconds = timegm(&utc);
#endif
    if (seconds == static_cast<std::time_t>(-1))
    {
        return false;
    }

    out = std::chrono::system_clock::from_time_t(seconds) + std::chrono::milliseconds{millis};
    return true;
}

}
