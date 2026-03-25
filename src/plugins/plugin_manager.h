#pragma once

#include <string>
#include <vector>

namespace zibby::plugins {

class PluginManager {
public:
    bool loadFromDirectory(const std::string& directory);

private:
    std::vector<std::string> loaded_;
};

} // namespace zibby::plugins
