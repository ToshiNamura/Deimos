#include "config_store.hpp"

#include <filesystem>
#include <random>
#include <utility>

#include <deimos/support/log.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

std::string trimToken(std::string value)
{
    return strings::trim(value);
}

} // namespace

std::string generateToken()
{
    std::random_device device;
    std::uniform_int_distribution<int> distribution(0, 255);

    std::string bytes;
    bytes.reserve(32);
    for (int index = 0; index < 32; ++index)
    {
        bytes.push_back(static_cast<char>(distribution(device)));
    }
    return strings::toHex(bytes);
}

ConfigStore::ConfigStore(std::string path)
    : path_(std::move(path))
{
}

Status ConfigStore::load(std::vector<Diagnostic>* diagnostics)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const std::filesystem::path filePath{path_};
    platform::ensureDirectory(filePath.parent_path());

    const auto content = platform::readFile(filePath);
    if (!content.has_value())
    {
        config_ = defaultConfig();
        loaded_ = true;
        log::warning("config", "no configuration at " + path_ + ", writing defaults");
        return saveLocked(diagnostics);
    }

    DeimosConfig config;
    std::vector<Diagnostic> localDiagnostics;
    const Status status = deserializeConfig(*content, config, localDiagnostics);
    if (diagnostics != nullptr)
    {
        diagnostics->insert(diagnostics->end(), localDiagnostics.begin(), localDiagnostics.end());
    }
    if (!status.isOk())
    {
        return status;
    }

    config_ = std::move(config);
    loaded_ = true;
    log::info("config", "loaded configuration from " + path_);
    return Status::ok();
}

Status ConfigStore::save(std::vector<Diagnostic>* diagnostics)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return saveLocked(diagnostics);
}

Status ConfigStore::saveLocked(std::vector<Diagnostic>* diagnostics)
{
    const auto validationDiagnostics = validateConfig(config_);
    if (diagnostics != nullptr)
    {
        diagnostics->insert(diagnostics->end(), validationDiagnostics.begin(), validationDiagnostics.end());
    }
    if (hasErrors(validationDiagnostics))
    {
        return Status::failure(ErrorCode::configuration,
                               "configuration is invalid",
                               formatDiagnostics(validationDiagnostics));
    }

    const std::filesystem::path filePath{path_};
    platform::ensureDirectory(filePath.parent_path());

    std::string errorMessage;
    if (!platform::writeFileAtomically(filePath, serializeConfig(config_, true) + "\n", errorMessage))
    {
        return Status::failure(ErrorCode::io, "cannot write configuration", errorMessage);
    }
    return Status::ok();
}

DeimosConfig ConfigStore::config() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ConfigStore::setConfig(DeimosConfig config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = std::move(config);
}

const std::string& ConfigStore::path() const
{
    return path_;
}

std::string ConfigStore::tokenFile() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.daemon.tokenFile.empty())
    {
        return config_.daemon.tokenFile;
    }
    return defaultTokenPath();
}

Result<std::string> ConfigStore::ensureToken()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!token_.empty())
        {
            return Result<std::string>{token_};
        }
        if (!config_.daemon.requireAuthentication)
        {
            return Result<std::string>{std::string{}};
        }
    }

    const std::string tokenPath = tokenFile();
    if (const auto existing = platform::readFile(tokenPath); existing.has_value())
    {
        const std::string trimmed = trimToken(*existing);
        if (!trimmed.empty())
        {
            std::lock_guard<std::mutex> lock(mutex_);
            token_ = trimmed;
            return Result<std::string>{token_};
        }
    }

    return rotateToken();
}

std::string ConfigStore::token() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return token_;
}

Result<std::string> ConfigStore::rotateToken()
{
    const std::string tokenPath = tokenFile();
    const std::string token = generateToken();

    const std::filesystem::path filePath{tokenPath};
    platform::ensureDirectory(filePath.parent_path());

    std::string errorMessage;
    if (!platform::writeFileAtomically(filePath, token + "\n", errorMessage))
    {
        return Result<std::string>{
            Error{ErrorCode::io, "cannot write the access token", errorMessage}};
    }
    platform::restrictFilePermissions(filePath);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        token_ = token;
    }
    log::info("config", "access token written to " + tokenPath);
    return Result<std::string>{token};
}

}
