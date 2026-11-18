#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/support/error.hpp>

namespace deimos
{

/// Owns the configuration file and the bearer token on disk.
class ConfigStore
{
public:
    explicit ConfigStore(std::string path);

    /// Loads the configuration, creating a default file when none exists.
    Status load(std::vector<Diagnostic>* diagnostics = nullptr);

    /// Validates and writes the configuration back atomically.
    Status save(std::vector<Diagnostic>* diagnostics = nullptr);

    /// Thread-safe copy of the current configuration.
    DeimosConfig config() const;
    void setConfig(DeimosConfig config);

    const std::string& path() const;

    /// Effective token path: the configured one or the platform default.
    std::string tokenFile() const;

    /// Returns the token, creating and persisting one on first use.
    Result<std::string> ensureToken();

    /// Cached token; empty when none has been loaded or generated yet.
    std::string token() const;

    /// Generates a new token, stores it and returns it.
    Result<std::string> rotateToken();

private:
    Status saveLocked(std::vector<Diagnostic>* diagnostics);

    mutable std::mutex mutex_;
    std::string path_;
    DeimosConfig config_;
    std::string token_;
    bool loaded_ = false;
};

/// Random URL-safe bearer token (32 bytes, hex encoded).
std::string generateToken();

}
