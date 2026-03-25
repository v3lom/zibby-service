#include "core/config.h"
#include "core/crypto.h"
#include "core/database.h"
#include "core/message.h"
#include "core/service.h"

#include <boost/program_options.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    namespace po = boost::program_options;

    po::options_description options("Zibby Service options");
    options.add_options()
        ("help,h", "Show help")
        ("daemon,d", "Run as daemon/background service")
        ("cli,c", "Connect CLI to running service")
        ("version,v", "Show version")
        ("send", "Send text message")
        ("edit", "Edit message by id")
        ("read", "Mark message as read by id")
        ("history", "Show chat history")
        ("chat", po::value<std::string>(), "Chat id")
        ("from", po::value<std::string>(), "Sender id")
        ("to", po::value<std::string>(), "Recipient id")
        ("message,m", po::value<std::string>(), "Message text")
        ("id", po::value<long long>(), "Message id")
        ("limit", po::value<int>()->default_value(20), "History limit");

    po::variables_map variables;
    try {
        po::store(po::parse_command_line(argc, argv, options), variables);
        po::notify(variables);
    } catch (const std::exception& ex) {
        std::cerr << "Argument error: " << ex.what() << '\n';
        std::cout << options << '\n';
        return 1;
    }

    if (variables.count("help") > 0) {
        std::cout << options << '\n';
        return 0;
    }

    if (variables.count("version") > 0) {
        std::cout << "zibby-service " << ZIBBY_VERSION << '\n';
        return 0;
    }

    zibby::core::ConfigManager configManager;
    const auto config = configManager.loadOrCreate();
    zibby::core::Database database;
    zibby::core::Crypto crypto;

    if (!database.initialize(config.dbFile)) {
        std::cerr << "Database initialization failed" << '\n';
        return 2;
    }

    const std::string storageSecret = config.dataDir + ":zibby-storage-secret";
    zibby::core::MessageService messageService(database, crypto, storageSecret);

    if (variables.count("send") > 0) {
        if (variables.count("chat") == 0 || variables.count("from") == 0 || variables.count("to") == 0 || variables.count("message") == 0) {
            std::cerr << "Missing required flags for --send: --chat --from --to --message" << '\n';
            return 1;
        }

        const auto id = messageService.sendText(
            variables["chat"].as<std::string>(),
            variables["from"].as<std::string>(),
            variables["to"].as<std::string>(),
            variables["message"].as<std::string>());

        if (!id.has_value()) {
            std::cerr << "Message send failed" << '\n';
            return 1;
        }

        std::cout << "Message sent. id=" << *id << '\n';
        return 0;
    }

    if (variables.count("edit") > 0) {
        if (variables.count("id") == 0 || variables.count("message") == 0) {
            std::cerr << "Missing required flags for --edit: --id --message" << '\n';
            return 1;
        }

        const bool ok = messageService.editMessage(variables["id"].as<long long>(), variables["message"].as<std::string>());
        std::cout << (ok ? "Message edited" : "Edit failed") << '\n';
        return ok ? 0 : 1;
    }

    if (variables.count("read") > 0) {
        if (variables.count("id") == 0) {
            std::cerr << "Missing required flags for --read: --id" << '\n';
            return 1;
        }

        const bool ok = messageService.markRead(variables["id"].as<long long>());
        std::cout << (ok ? "Message marked as read" : "Read update failed") << '\n';
        return ok ? 0 : 1;
    }

    if (variables.count("history") > 0) {
        if (variables.count("chat") == 0) {
            std::cerr << "Missing required flags for --history: --chat" << '\n';
            return 1;
        }

        const auto history = messageService.history(variables["chat"].as<std::string>(), static_cast<std::size_t>(variables["limit"].as<int>()));
        for (const auto& message : history) {
            std::cout
                << message.id << " | "
                << message.from << " -> " << message.to << " | "
                << (message.read ? "read" : "unread") << " | "
                << (message.edited ? "edited" : "original") << " | "
                << message.payload << '\n';
        }
        return 0;
    }

    zibby::core::Service service(config);

    if (variables.count("cli") > 0) {
        if (service.pingRunningInstance()) {
            std::cout << "Connected: running instance is available" << '\n';
            return 0;
        }

        std::cout << "Service is not running" << '\n';
        return 2;
    }

    const bool daemonMode = variables.count("daemon") > 0;
    return service.run(daemonMode);
}
