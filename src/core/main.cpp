#include "core/config.h"
#include "core/crypto.h"
#include "core/database.h"
#include "core/message.h"
#include "core/network.h"
#include "core/profile.h"
#include "core/service.h"

#include "version.h"

#include "cli/cli.h"
#include "cli/tui.h"

#include <boost/program_options.hpp>
#include <boost/asio/ip/host_name.hpp>

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    namespace po = boost::program_options;

    po::options_description options("Zibby Service options");
    options.add_options()
        ("help,h", "Show help")
        ("daemon,d", "Run as daemon/background service")
        ("cli,c", "Connect CLI to running service")
        ("version,v", "Show version")
        ("api-info", "Show local API endpoint and token")
        ("send", "Send text message")
        ("edit", "Edit message by id")
        ("read", "Mark message as read by id")
        ("history", "Show chat history")
        ("tui", "Run interactive messenger TUI")
        ("profile-show", "Show local profile")
        ("profile-set-name", po::value<std::string>(), "Set profile display name")
        ("profile-set-bio", po::value<std::string>(), "Set profile bio")
        ("profile-set-status", po::value<std::string>(), "Set profile status: active|hidden|inactive")
        ("discover", "Discover clients in local network")
        ("peers", "List cached discovered peers")
        ("peer-add", po::value<std::string>(), "Add peer manually: host:port:name")
        ("discover-timeout", po::value<int>()->default_value(1200), "Discovery timeout in milliseconds")
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
        std::cout << "zibby-service " << ZIBBY_VERSION_STRING << " by " << ZIBBY_AUTHOR_STRING << '\n';
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
    zibby::core::ProfileService profileService(database);
    profileService.ensureLocalProfile(boost::asio::ip::host_name());

    if (variables.count("profile-show") > 0) {
        const auto profile = profileService.get();
        if (!profile.has_value()) {
            std::cerr << "Profile not found" << '\n';
            return 1;
        }

        std::cout << "id=" << profile->id << '\n';
        std::cout << "name=" << profile->displayName << '\n';
        std::cout << "status=" << profile->status << '\n';
        std::cout << "bio=" << profile->bio << '\n';
        return 0;
    }

    if (variables.count("profile-set-name") > 0 || variables.count("profile-set-bio") > 0 || variables.count("profile-set-status") > 0) {
        auto profile = profileService.get().value_or(profileService.ensureLocalProfile(boost::asio::ip::host_name()));
        if (variables.count("profile-set-name") > 0) {
            profile.displayName = variables["profile-set-name"].as<std::string>();
        }
        if (variables.count("profile-set-bio") > 0) {
            profile.bio = variables["profile-set-bio"].as<std::string>();
        }
        if (variables.count("profile-set-status") > 0) {
            profile.status = variables["profile-set-status"].as<std::string>();
        }

        if (!profileService.save(profile)) {
            std::cerr << "Profile update failed" << '\n';
            return 1;
        }

        std::cout << "Profile updated" << '\n';
        return 0;
    }

    if (variables.count("discover") > 0) {
        const auto peers = zibby::core::Network::discoverPeers(config.listenPort, variables["discover-timeout"].as<int>());
        for (const auto& peer : peers) {
            database.upsertPeer(peer);
        }

        std::cout << "Discovered peers: " << peers.size() << '\n';
        for (const auto& peer : peers) {
            std::cout << peer.peerId << " | " << peer.displayName << " | " << peer.host << ":" << peer.port << " | " << peer.version << '\n';
        }
        return 0;
    }

    if (variables.count("peer-add") > 0) {
        const std::string raw = variables["peer-add"].as<std::string>();
        const auto first = raw.find(':');
        const auto second = raw.find(':', first == std::string::npos ? 0 : first + 1);
        if (first == std::string::npos || second == std::string::npos) {
            std::cerr << "Invalid --peer-add format. Use host:port:name" << '\n';
            return 1;
        }

        zibby::core::PeerInfo peer;
        peer.host = raw.substr(0, first);
        try {
            peer.port = std::stoi(raw.substr(first + 1, second - first - 1));
        } catch (...) {
            std::cerr << "Invalid peer port" << '\n';
            return 1;
        }
        peer.displayName = raw.substr(second + 1);
        peer.peerId = peer.host + ":" + std::to_string(peer.port);
        peer.version = "manual";

        if (!database.upsertPeer(peer)) {
            std::cerr << "Unable to store peer" << '\n';
            return 1;
        }

        std::cout << "Peer saved" << '\n';
        return 0;
    }

    if (variables.count("peers") > 0) {
        const auto peers = database.listPeers();
        for (const auto& peer : peers) {
            std::cout << peer.peerId << " | " << peer.displayName << " | " << peer.host << ":" << peer.port << " | " << peer.version << '\n';
        }
        return 0;
    }

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

    if (variables.count("tui") > 0) {
        zibby::cli::Tui tui;
        return tui.run(messageService, profileService, database, config.listenPort);
    }

    zibby::core::Service service(config);

    if (variables.count("api-info") > 0) {
        std::cout << "api_endpoint=" << service.apiEndpoint() << '\n';
        const auto token = service.apiToken();
        std::cout << "api_token=" << (token.empty() ? "<not-generated-yet-start-service-first>" : token) << '\n';
        return 0;
    }

    if (variables.count("cli") > 0) {
        if (!service.pingRunningInstance()) {
            std::cout << "Service is not running" << '\n';
            return 2;
        }

        zibby::cli::Cli cli;
        return cli.run(service.apiEndpoint(), service.apiToken());
    }

    const bool daemonMode = variables.count("daemon") > 0;
    return service.run(daemonMode);
}
