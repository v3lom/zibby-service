#pragma once

#include "core/config.h"

#include <string>

namespace zibby::core {

class Service {
public:
    explicit Service(Config config);

    int run(bool daemonMode);
    bool pingRunningInstance() const;
    std::string apiToken() const;
    std::string apiEndpoint() const;

private:
    Config config_;
};

} // namespace zibby::core
