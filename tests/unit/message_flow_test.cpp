#include "core/crypto.h"
#include "core/database.h"
#include "core/message.h"

#include <boost/filesystem.hpp>

#include <cstdlib>
#include <iostream>

int main() {
    namespace fs = boost::filesystem;

    const fs::path baseDir = fs::temp_directory_path() / fs::unique_path("zibby-message-flow-test-%%%%-%%%%-%%%%");
    fs::create_directories(baseDir);
    const fs::path dbPath = baseDir / "messages.sqlite3";

    {
        zibby::core::Database database;
        if (!database.initialize(dbPath.string())) {
            std::cerr << "database.initialize failed" << std::endl;
            return EXIT_FAILURE;
        }

        zibby::core::Crypto crypto;
        zibby::core::MessageService service(database, crypto, "unit-test-secret");

        const auto sentId = service.sendText("chat-1", "alice", "bob", "hello world");
        if (!sentId.has_value()) {
            std::cerr << "sendText failed" << std::endl;
            return EXIT_FAILURE;
        }

        if (!service.editMessage(*sentId, "edited world")) {
            std::cerr << "editMessage failed" << std::endl;
            return EXIT_FAILURE;
        }

        if (!service.markRead(*sentId)) {
            std::cerr << "markRead failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto history = service.history("chat-1", 10);
        if (history.size() != 1) {
            std::cerr << "unexpected history size" << std::endl;
            return EXIT_FAILURE;
        }

        const auto& first = history.front();
        if (first.payload != "edited world" || !first.read || !first.edited) {
            std::cerr << "unexpected message fields" << std::endl;
            return EXIT_FAILURE;
        }
    }

    fs::remove_all(baseDir);
    return EXIT_SUCCESS;
}
