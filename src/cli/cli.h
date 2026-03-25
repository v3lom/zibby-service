#pragma once

#include <string>

namespace zibby::cli {

class Cli {
public:
    int run();
    int run(const std::string& apiEndpoint, const std::string& apiToken);
};

} // namespace zibby::cli
