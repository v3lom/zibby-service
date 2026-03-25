#pragma once

#include <string>

namespace zibby::core {

class Crypto {
public:
    bool ensureUserKeys(const std::string& dataDir) const;
};

} // namespace zibby::core
