#pragma once

#include <string>

namespace zibby::core {

struct Config {
    int listenPort = 9876;
    int multicastPort = 9876;
    std::string multicastGroup = "239.0.0.1";
    std::string logFile = "zibby.log";
    std::string dataDir;
    std::string configPath;
    std::string dbFile;
};

class ConfigManager {
public:
    Config loadOrCreate();
    static std::string defaultConfigPath();
    static std::string defaultDataDir();
};

} // namespace zibby::core
