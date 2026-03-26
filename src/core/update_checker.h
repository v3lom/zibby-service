#pragma once

#include <string>

namespace zibby::core {

struct UpdateCheckResult {
    bool ok = false;
    bool updateAvailable = false;
    std::string latestVersion;
    std::string error;
};

class UpdateChecker {
public:
    // Checks GitHub latest release for repository given by URL (e.g. https://github.com/v3lom/zibby-service)
    static UpdateCheckResult checkLatestRelease(const std::string& repoUrl, const std::string& currentVersion);

private:
    static bool parseRepoUrl(const std::string& repoUrl, std::string* owner, std::string* repo);
};

} // namespace zibby::core
