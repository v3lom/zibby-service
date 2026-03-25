#pragma once

#include <string>

namespace zibby::core {

struct Config {
    int listenPort = 9876;
    int controlPort = 9877;
    int apiPort = 9888;
    int multicastPort = 9876;
    std::string multicastGroup = "239.0.0.1";
    std::string logFile = "zibby.log";
    std::string dataDir;
    std::string cacheDir;
    std::string downloadDir;
    std::string configPath;
    std::string dbFile;
    bool updatesEnabled = true;
    std::string updatesRepoUrl = "https://github.com/v3lom/zibby-service";
    int updatesCheckIntervalHours = 24;
};

class ConfigManager {
public:
    Config loadOrCreate();
    static std::string defaultConfigPath();
    static std::string defaultDataDir();
    static std::string defaultCacheDir();
    static std::string defaultDownloadDir();
};

} // namespace zibby::core
