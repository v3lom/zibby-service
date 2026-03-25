#pragma once

namespace zibby::core {
class Database;
struct Config;
class MessageService;
class ProfileService;
}

namespace zibby::cli {

class Tui {
public:
    int run(
        zibby::core::MessageService& messageService,
        zibby::core::ProfileService& profileService,
        zibby::core::Database& database,
        const zibby::core::Config& config);
};

} // namespace zibby::cli
