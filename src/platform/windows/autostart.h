#pragma once

#ifdef _WIN32

#include <string>

namespace zibby::platform::windows {

struct AutostartResult {
    bool ok = false;
    std::string error;
};

AutostartResult enableAutostartRunKey(const std::string& valueName, const std::string& commandLine);
AutostartResult disableAutostartRunKey(const std::string& valueName);

} // namespace zibby::platform::windows

#endif
