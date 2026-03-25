#pragma once

#include <string>

namespace zibby::core {

/**
 * @brief Информация об обнаруженном клиенте.
 */
struct PeerInfo {
    std::string peerId;
    std::string displayName;
    std::string host;
    int port = 0;
    std::string version;
    std::string lastSeen;
};

} // namespace zibby::core
