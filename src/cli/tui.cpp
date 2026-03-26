#include "cli/tui.h"

#include "utils/ansi.h"

#include "core/database.h"
#include "core/message.h"
#include "core/network.h"
#include "core/profile.h"
#include "core/config.h"

#include <boost/asio/ip/host_name.hpp>
#include <boost/filesystem.hpp>

#include <fstream>
#include <cctype>
#include <iostream>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

enum class Lang {
    En,
    Ru,
};

Lang detectLang() {
    const char* lang = std::getenv("LANG");
    if (lang != nullptr) {
        std::string v(lang);
        for (auto& ch : v) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (v.rfind("ru", 0) == 0 || v.find("_ru") != std::string::npos || v.find("ru_") != std::string::npos) {
            return Lang::Ru;
        }
    }
#ifdef _WIN32
    const LANGID lid = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(lid) == LANG_RUSSIAN) {
        return Lang::Ru;
    }
#endif
    return Lang::En;
}

struct Strings {
    Lang lang = Lang::En;

    const char* title() const { return lang == Lang::Ru ? "Zibby TUI" : "Zibby TUI"; }
    const char* menuProfile() const { return lang == Lang::Ru ? "Профиль" : "Profile"; }
    const char* menuSend() const { return lang == Lang::Ru ? "Отправить сообщение" : "Send message"; }
    const char* menuHistory() const { return lang == Lang::Ru ? "История чата" : "Chat history"; }
    const char* menuDiscover() const { return lang == Lang::Ru ? "Поиск клиентов" : "Discover peers"; }
    const char* menuPeers() const { return lang == Lang::Ru ? "Список клиентов" : "Peers list"; }
    const char* menuClearCache() const { return lang == Lang::Ru ? "Очистка кеша" : "Clear cache"; }
    const char* menuUnsupported() const {
        return lang == Lang::Ru ? "Неподдерживаемые действия (звонки/файлы)" : "Unsupported actions (calls/files)";
    }
    const char* menuExit() const { return lang == Lang::Ru ? "Выход" : "Exit"; }

    const char* prompt() const { return lang == Lang::Ru ? "> " : "> "; }
    const char* currentName() const { return lang == Lang::Ru ? "Текущее имя" : "Current name"; }
    const char* newNamePrompt() const { return lang == Lang::Ru ? "Новое имя (Enter чтобы оставить)" : "New name (Enter to keep)"; }
    const char* profileSaveError() const { return lang == Lang::Ru ? "Ошибка сохранения профиля" : "Profile save error"; }
    const char* profileUpdated() const { return lang == Lang::Ru ? "Профиль обновлен" : "Profile updated"; }

    const char* sendError() const { return lang == Lang::Ru ? "Ошибка отправки" : "Send failed"; }
    const char* sentOk() const { return lang == Lang::Ru ? "Отправлено" : "Sent"; }

    const char* foundPeers() const { return lang == Lang::Ru ? "Найдено клиентов" : "Peers found"; }
    const char* cacheCleared() const { return lang == Lang::Ru ? "Кеш очищен" : "Cache cleared"; }
    const char* cacheClearError() const { return lang == Lang::Ru ? "Ошибка очистки кеша" : "Cache clear error"; }

    const char* unsupportedText1() const {
        return lang == Lang::Ru
            ? "Звонки, голосовые сообщения и передача файлов пока не поддерживаются в CLI/TUI."
            : "Calls, voice messages and file transfer are not supported in CLI/TUI yet.";
    }
    const char* unsupportedText2() const {
        return lang == Lang::Ru
            ? "Используйте внешний фронтенд через локальный API."
            : "Use an external frontend via the local API.";
    }
};

} // namespace

namespace zibby::cli {

int Tui::run(
    zibby::core::MessageService& messageService,
    zibby::core::ProfileService& profileService,
    zibby::core::Database& database,
    const zibby::core::Config& config) {
    namespace fs = boost::filesystem;

    const auto out = zibby::utils::Ansi::forStdout();
    Strings strings;
    strings.lang = detectLang();

    while (true) {
        std::cout << "\n" << out.bold() << out.cyan() << "=== " << strings.title() << " ===" << out.reset() << "\n";
        std::cout << "1) " << strings.menuProfile() << "\n";
        std::cout << "2) " << strings.menuSend() << "\n";
        std::cout << "3) " << strings.menuHistory() << "\n";
        std::cout << "4) " << strings.menuDiscover() << "\n";
        std::cout << "5) " << strings.menuPeers() << "\n";
        std::cout << "6) " << strings.menuClearCache() << "\n";
        std::cout << "7) " << strings.menuUnsupported() << "\n";
        std::cout << "0) " << strings.menuExit() << "\n";
        std::cout << strings.prompt();

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "0") {
            return 0;
        }

        if (choice == "1") {
            auto profile = profileService.ensureLocalProfile(boost::asio::ip::host_name());
            std::cout << strings.currentName() << ": " << profile.displayName << "\n";
            std::cout << strings.newNamePrompt() << ": ";
            std::string newName;
            std::getline(std::cin, newName);
            if (!newName.empty()) {
                profile.displayName = newName;
                if (!profileService.save(profile)) {
                    std::cout << out.red() << strings.profileSaveError() << out.reset() << "\n";
                } else {
                    std::cout << out.green() << strings.profileUpdated() << out.reset() << "\n";
                }
            }
            continue;
        }

        if (choice == "2") {
            std::string chat;
            std::string from;
            std::string to;
            std::string text;
            std::cout << "chat id: ";
            std::getline(std::cin, chat);
            std::cout << "from: ";
            std::getline(std::cin, from);
            std::cout << "to: ";
            std::getline(std::cin, to);
            std::cout << "text: ";
            std::getline(std::cin, text);

            const auto id = messageService.sendText(chat, from, to, text);
            if (!id.has_value()) {
                std::cout << out.red() << strings.sendError() << out.reset() << "\n";
            } else {
                std::cout << out.green() << strings.sentOk() << out.reset() << ", id=" << *id << "\n";
            }
            continue;
        }

        if (choice == "3") {
            std::string chat;
            std::cout << "chat id: ";
            std::getline(std::cin, chat);
            const auto history = messageService.history(chat, 50);
            for (const auto& message : history) {
                std::cout << message.id << " | " << message.from << " -> " << message.to << " | " << message.payload << "\n";
            }
            continue;
        }

        if (choice == "4") {
            const auto peers = zibby::core::Network::discoverPeers(config.listenPort, 1200);
            for (const auto& peer : peers) {
                database.upsertPeer(peer);
            }
            std::cout << strings.foundPeers() << ": " << peers.size() << "\n";
            continue;
        }

        if (choice == "5") {
            const auto peers = database.listPeers(100);
            for (const auto& peer : peers) {
                std::cout << peer.peerId << " | " << peer.displayName << " | " << peer.host << ":" << peer.port << " | " << peer.version << "\n";
            }
            continue;
        }

        if (choice == "6") {
            const fs::path cacheDir(config.cacheDir);
            boost::system::error_code ec;
            std::uintmax_t removedCount = 0;
            if (fs::exists(cacheDir, ec)) {
                removedCount = fs::remove_all(cacheDir, ec);
            }
            if (ec) {
                std::cout << out.red() << strings.cacheClearError() << out.reset() << ": " << ec.message() << "\n";
            } else {
                std::cout << out.green() << strings.cacheCleared() << out.reset() << ". Removed: " << removedCount << "\n";
            }
            continue;
        }

        if (choice == "7") {
            std::cout << out.yellow() << strings.unsupportedText1() << out.reset() << "\n";
            std::cout << strings.unsupportedText2() << "\n";
            continue;
        }
    }

    return 0;
}

} // namespace zibby::cli
