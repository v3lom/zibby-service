#include "core/config.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cstdlib>
#include <fstream>

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace zibby::core {

namespace {
void writeDefaultConfig(const fs::path& path) {
    fs::create_directories(path.parent_path());

    std::ofstream out(path.string(), std::ios::trunc);
    out << R"({
  "version": 1,
  "user": {
    "name": "",
    "avatar": "",
    "bio": "",
    "birth_date": "",
    "status": "active"
  },
  "network": {
    "listen_port": 9876,
    "multicast_group": "239.0.0.1",
    "multicast_port": 9876,
    "broadcast_interval": 30,
    "interfaces": [],
    "extra_peers": []
  },
  "storage": {
    "data_dir": "",
    "cache_dir": "",
    "download_dir": "",
    "max_cache_size_mb": 1024
  },
  "crypto": {
    "algorithm": "aes-256-gcm",
    "key_length": 32
  },
  "logging": {
    "level": "info",
    "file": "zibby.log",
    "max_size_mb": 10,
    "max_files": 5
  },
  "updates": {
    "enabled": true,
    "repo_url": "https://github.com/v3lom/zibby-service",
    "check_interval_hours": 24
  },
  "autostart": true,
  "performance_mode": "balanced"
})";
}
} // namespace

std::string ConfigManager::defaultDataDir() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr) {
        return (fs::path(appData) / "Zibby").string();
    }
    return (fs::temp_directory_path() / "Zibby").string();
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return (fs::path(home) / ".local" / "share" / "zibby").string();
    }
    return (fs::temp_directory_path() / "zibby").string();
#endif
}

std::string ConfigManager::defaultConfigPath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr) {
        return (fs::path(appData) / "Zibby" / "config.json").string();
    }
    return (fs::temp_directory_path() / "Zibby" / "config.json").string();
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return (fs::path(home) / ".config" / "zibby" / "config.json").string();
    }
    return (fs::temp_directory_path() / "zibby" / "config.json").string();
#endif
}

Config ConfigManager::loadOrCreate() {
    Config config;
    config.configPath = defaultConfigPath();

    const fs::path configPath(config.configPath);
    if (!fs::exists(configPath)) {
        writeDefaultConfig(configPath);
    }

    pt::ptree tree;
    pt::read_json(config.configPath, tree);

    config.listenPort = tree.get<int>("network.listen_port", 9876);
    config.multicastPort = tree.get<int>("network.multicast_port", 9876);
    config.multicastGroup = tree.get<std::string>("network.multicast_group", "239.0.0.1");
    config.logFile = tree.get<std::string>("logging.file", "zibby.log");

    const auto configuredDataDir = tree.get<std::string>("storage.data_dir", "");
    config.dataDir = configuredDataDir.empty() ? defaultDataDir() : configuredDataDir;

    fs::create_directories(config.dataDir);
    config.dbFile = (fs::path(config.dataDir) / "zibby.sqlite3").string();

    return config;
}

} // namespace zibby::core
