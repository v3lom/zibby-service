#pragma once

#include <string>

namespace zibby::core {

class Crypto {
public:
    bool ensureUserKeys(const std::string& dataDir) const;
    std::string encryptForStorage(const std::string& plaintext, const std::string& secret) const;
    std::string decryptFromStorage(const std::string& ciphertextBase64, const std::string& secret) const;
};

} // namespace zibby::core
