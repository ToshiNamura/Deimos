#pragma once

#include <functional>
#include <string>
#include <vector>

#include <deimos/support/error.hpp>

namespace deimos::service
{

struct InstallOptions
{
    std::string serviceName{"deimosd"};
    std::string displayName{"Deimos DPI bypass daemon"};
    std::string description{"Modifies outgoing traffic with zapret under the control of Deimos."};
    std::string executablePath;
    std::string configPath;
    std::vector<std::string> arguments;
    bool startImmediately = true;
};

/// Installs and optionally starts the platform service. \p message carries instructions.
Status install(const InstallOptions& options, std::string& message);

/// Removes the platform service.
Status uninstall(const std::string& serviceName, std::string& message);

/// systemd unit written by the Linux installer.
std::string renderSystemdUnit(const InstallOptions& options);

/// launchd plist written by the macOS installer.
std::string renderLaunchdPlist(const InstallOptions& options);

/// True when the process was started by the Windows service control manager.
bool isRunningAsService();

/// Serves the Windows SCM. Returns false when the process was not started by the SCM.
bool runAsService(const std::string& serviceName,
                  const std::function<int()>& runBlocking,
                  const std::function<void()>& requestStop,
                  std::string& errorMessage);

}
