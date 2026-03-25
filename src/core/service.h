#pragma once

#include "core/config.h"

namespace zibby::core {

class Service {
public:
    explicit Service(Config config);

    int run(bool daemonMode);
    bool pingRunningInstance() const;

private:
    Config config_;
};

} // namespace zibby::core
