#include "core/config.h"
#include "core/crypto.h"
#include "core/database.h"
#include "core/message.h"
#include "core/network.h"
#include "core/profile.h"
#include "core/service.h"
#include "core/update_checker.h"

#ifdef _WIN32
#include "platform/windows/windows_service.h"
#include "platform/windows/autostart.h"
#endif

#ifdef ZIBBY_HAS_PANEL
#include "panel/panel_entry.h"
#endif

#include "version.h"

#include "cli/cli.h"
#include "cli/tui.h"

#include <boost/program_options.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/filesystem.hpp>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef _WIN32
namespace {

std::string exeDir() {
    char buffer[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }

    std::string path(buffer, buffer + len);
    const auto pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return {};
    }
    return path.substr(0, pos);
}

void hideConsoleWindow() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd != nullptr) {
        ShowWindow(hwnd, SW_HIDE);
    }
    FreeConsole();
}

bool spawnDetachedSelf(const std::string& args) {
    char buffer[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    std::string cmdLine = std::string("\"") + buffer + "\" " + args;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const BOOL ok = CreateProcessA(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | DETACHED_PROCESS,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok) {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

std::string findInstallerExe(const std::string& baseDir) {
    namespace fs = boost::filesystem;
    std::vector<fs::path> roots;
    roots.push_back(fs::path(baseDir) / "installers");
    roots.push_back(fs::path(baseDir) / ".." / "installers");

    fs::path best;
    std::time_t bestTime = 0;

    for (const auto& root : roots) {
        boost::system::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
            continue;
        }
        for (fs::directory_iterator it(root, ec); !ec && it != fs::directory_iterator(); ++it) {
            const auto p = it->path();
            if (!fs::is_regular_file(p, ec)) {
                continue;
            }
            if (p.extension().string() != ".exe") {
                continue;
            }
            const auto name = p.filename().string();
            if (name.find("zibby") == std::string::npos) {
                continue;
            }
            const auto t = fs::last_write_time(p, ec);
            if (!ec && (best.empty() || t > bestTime)) {
                best = p;
                bestTime = t;
            }
        }
    }

    return best.empty() ? std::string{} : best.string();
}

void offerRunInstaller(const std::string& installerPath) {
    const int res = MessageBoxA(
        nullptr,
        "zibby-service is not installed or GUI panel is missing.\n\nRun installer now?",
        "zibby-service",
        MB_YESNO | MB_ICONQUESTION);
    if (res == IDYES) {
        ShellExecuteA(nullptr, "open", installerPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

} // namespace
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Windows launcher behavior:
    // - No args: start service in background if needed.
    //           If already running, open embedded Qt panel (full build).
    // - With args: keep console for CLI/TUI.
    if (argc <= 1) {
        hideConsoleWindow();

        zibby::core::ConfigManager configManager;
        const auto config = configManager.loadOrCreate();
        zibby::core::Service service(config);

        if (service.pingRunningInstance()) {
#ifdef ZIBBY_HAS_PANEL
            return zibby::panel::runPanel(argc, argv);
#else
            const auto dir = exeDir();
            const auto installer = findInstallerExe(dir);
            if (!installer.empty()) {
                offerRunInstaller(installer);
            } else {
                MessageBoxA(nullptr, "GUI is not available in this build and installer not located.", "zibby-service", MB_OK | MB_ICONWARNING);
            }
            return 0;
#endif
        }

        // First run: start daemon and exit. Second run will open GUI.
        if (!service.pingRunningInstance()) {
            spawnDetachedSelf("--daemon");
        }
        return 0;
    }

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
        ("service", "Run under Windows Service Control Manager")
        ("install-service", "Install Windows service (requires admin)")
        ("uninstall-service", "Uninstall Windows service (requires admin)")
        ("start-service", "Start Windows service")
        ("stop-service", "Stop Windows service")
        ("install-best-effort", "Best-effort install: try Windows service, fallback to HKCU autostart")
        ("uninstall-best-effort", "Best-effort uninstall: try to remove service and disable HKCU autostart")
        ("enable-autostart", "Enable autostart for current user (HKCU Run -> --daemon)")
        ("disable-autostart", "Disable autostart for current user (HKCU Run)")
        ("panel", "Open embedded Qt panel (full build)")
        ("cli,c", "Connect CLI to running service")
        ("version,v", "Show version")
        ("check-updates", "Check for updates (GitHub latest release)")
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
        ("clear-cache", "Clear local cache directory")
        ("paths", "Show configured paths (data/cache/download/config/db)")
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

#ifdef _WIN32
    // Windows Service management commands (do not require config/db initialization).
    if (variables.count("install-best-effort") > 0) {
        char exePath[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        const std::string serviceBinPath = std::string("\"") + exePath + "\" --service";

        const auto install = zibby::platform::windows::installWindowsService(
            "zibby-service",
            "zibby-service",
            serviceBinPath);

        if (install.ok) {
            const auto start = zibby::platform::windows::startWindowsService("zibby-service");
            if (!start.ok) {
                std::cerr << "Start service failed: " << start.error << '\n';
                // Service installed but not started; still return non-zero.
                return 5;
            }
            std::cout << "Service installed and start requested" << '\n';
            return 0;
        }

        // Fallback: autostart (current user)
        const std::string autostartCmd = std::string("\"") + exePath + "\" --daemon";
        const auto as = zibby::platform::windows::enableAutostartRunKey("ZibbyService", autostartCmd);
        if (!as.ok) {
            std::cerr << "Install service failed: " << install.error << '\n';
            std::cerr << "Enable autostart failed: " << as.error << '\n';
            return 6;
        }
        std::cout << "Service install failed (" << install.error << "); autostart enabled (current user)" << '\n';
        return 0;
    }

    if (variables.count("uninstall-best-effort") > 0) {
        // Try to stop/uninstall service, but don't fail if it's not present.
        (void)zibby::platform::windows::stopWindowsService("zibby-service");
        (void)zibby::platform::windows::uninstallWindowsService("zibby-service");

        const auto as = zibby::platform::windows::disableAutostartRunKey("ZibbyService");
        if (!as.ok) {
            std::cerr << "Disable autostart failed: " << as.error << '\n';
            return 6;
        }

        std::cout << "Best-effort uninstall completed" << '\n';
        return 0;
    }

    if (variables.count("install-service") > 0) {
        char exePath[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        const std::string binPath = std::string("\"") + exePath + "\" --service";

        const auto r = zibby::platform::windows::installWindowsService(
            "zibby-service",
            "zibby-service",
            binPath);
        if (!r.ok) {
            std::cerr << "Install service failed: " << r.error << '\n';
            return 5;
        }
        std::cout << "Service installed" << '\n';
        return 0;
    }

    if (variables.count("uninstall-service") > 0) {
        const auto r = zibby::platform::windows::uninstallWindowsService("zibby-service");
        if (!r.ok) {
            std::cerr << "Uninstall service failed: " << r.error << '\n';
            return 5;
        }
        std::cout << "Service uninstalled" << '\n';
        return 0;
    }

    if (variables.count("start-service") > 0) {
        const auto r = zibby::platform::windows::startWindowsService("zibby-service");
        if (!r.ok) {
            std::cerr << "Start service failed: " << r.error << '\n';
            return 5;
        }
        std::cout << "Service start requested" << '\n';
        return 0;
    }

    if (variables.count("stop-service") > 0) {
        const auto r = zibby::platform::windows::stopWindowsService("zibby-service");
        if (!r.ok) {
            std::cerr << "Stop service failed: " << r.error << '\n';
            return 5;
        }
        std::cout << "Service stop requested" << '\n';
        return 0;
    }

    if (variables.count("enable-autostart") > 0) {
        char exePath[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        const std::string cmd = std::string("\"") + exePath + "\" --daemon";
        const auto r = zibby::platform::windows::enableAutostartRunKey("ZibbyService", cmd);
        if (!r.ok) {
            std::cerr << "Enable autostart failed: " << r.error << '\n';
            return 6;
        }
        std::cout << "Autostart enabled (current user)" << '\n';
        return 0;
    }

    if (variables.count("disable-autostart") > 0) {
        const auto r = zibby::platform::windows::disableAutostartRunKey("ZibbyService");
        if (!r.ok) {
            std::cerr << "Disable autostart failed: " << r.error << '\n';
            return 6;
        }
        std::cout << "Autostart disabled (current user)" << '\n';
        return 0;
    }
#endif

    zibby::core::ConfigManager configManager;
    const auto config = configManager.loadOrCreate();

#ifdef ZIBBY_HAS_PANEL
    if (variables.count("panel") > 0) {
        return zibby::panel::runPanel(argc, argv);
    }
#endif

#ifdef _WIN32
    if (variables.count("daemon") > 0) {
        // When started as a background process, don't keep an interactive console window.
        hideConsoleWindow();
    }
#endif

    if (variables.count("check-updates") > 0) {
        const auto r = zibby::core::UpdateChecker::checkLatestRelease(config.updatesRepoUrl, ZIBBY_VERSION_STRING);
        if (!r.ok) {
            std::cerr << "Update check failed: " << r.error << '\n';
            return 2;
        }
        std::cout << "latest=" << r.latestVersion << '\n';
        std::cout << "current=" << ZIBBY_VERSION_STRING << '\n';
        std::cout << "update_available=" << (r.updateAvailable ? "true" : "false") << '\n';
        return 0;
    }

#ifdef _WIN32
    if (variables.count("service") > 0) {
        // Run as a real Windows service.
        return zibby::platform::windows::runAsWindowsService(config);
    }
#endif
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

    if (variables.count("paths") > 0) {
        std::cout << "config=" << config.configPath << '\n';
        std::cout << "data=" << config.dataDir << '\n';
        std::cout << "cache=" << config.cacheDir << '\n';
        std::cout << "downloads=" << config.downloadDir << '\n';
        std::cout << "db=" << config.dbFile << '\n';
        return 0;
    }

    if (variables.count("clear-cache") > 0) {
        namespace fs = boost::filesystem;
        boost::system::error_code ec;
        std::uintmax_t removed = 0;
        if (fs::exists(config.cacheDir, ec)) {
            removed = fs::remove_all(config.cacheDir, ec);
        }
        if (ec) {
            std::cerr << "Cache cleanup failed: " << ec.message() << '\n';
            return 1;
        }
        fs::create_directories(config.cacheDir, ec);
        if (ec) {
            std::cerr << "Cache directory recreation failed: " << ec.message() << '\n';
            return 1;
        }
        std::cout << "Cache cleaned: " << removed << " entries removed from " << config.cacheDir << '\n';
        return 0;
    }

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
        return tui.run(messageService, profileService, database, config);
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
