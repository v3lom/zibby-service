#pragma once

namespace zibby::core {
class Database;
class MessageService;
class ProfileService;
}

namespace zibby::cli {

class Tui {
public:
    int run(zibby::core::MessageService& messageService, zibby::core::ProfileService& profileService, zibby::core::Database& database, int listenPort);
};

} // namespace zibby::cli
