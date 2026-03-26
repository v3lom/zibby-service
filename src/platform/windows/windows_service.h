#pragma once

#ifdef _WIN32

#include "core/config.h"

#include <string>

namespace zibby::platform::windows {

struct ServiceCommandResult {
    bool ok = false;
    std::string error;
};

// Runs the daemon under Windows Service Control Manager.
int runAsWindowsService(const zibby::core::Config& config);

ServiceCommandResult installWindowsService(const std::string& serviceName, const std::string& displayName, const std::string& exePathWithArgs);
ServiceCommandResult uninstallWindowsService(const std::string& serviceName);
ServiceCommandResult startWindowsService(const std::string& serviceName);
ServiceCommandResult stopWindowsService(const std::string& serviceName);

} // namespace zibby::platform::windows

#endif
