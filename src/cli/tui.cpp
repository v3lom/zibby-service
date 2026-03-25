#include "cli/tui.h"

#include "core/database.h"
#include "core/message.h"
#include "core/network.h"
#include "core/profile.h"
#include "core/config.h"

#include <boost/asio/ip/host_name.hpp>
#include <boost/filesystem.hpp>

#include <fstream>
#include <iostream>
#include <string>

namespace zibby::cli {

int Tui::run(
    zibby::core::MessageService& messageService,
    zibby::core::ProfileService& profileService,
    zibby::core::Database& database,
    const zibby::core::Config& config) {
    namespace fs = boost::filesystem;

    while (true) {
        std::cout << "\n=== Zibby TUI ===\n";
        std::cout << "1) Профиль\n";
        std::cout << "2) Отправить сообщение\n";
        std::cout << "3) История чата\n";
        std::cout << "4) Поиск клиентов\n";
        std::cout << "5) Список клиентов\n";
        std::cout << "6) Очистка кеша\n";
        std::cout << "7) Неподдерживаемые действия (звонки/файлы)\n";
        std::cout << "0) Выход\n";
        std::cout << "> ";

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "0") {
            return 0;
        }

        if (choice == "1") {
            auto profile = profileService.ensureLocalProfile(boost::asio::ip::host_name());
            std::cout << "Текущее имя: " << profile.displayName << "\n";
            std::cout << "Новое имя (Enter чтобы оставить): ";
            std::string newName;
            std::getline(std::cin, newName);
            if (!newName.empty()) {
                profile.displayName = newName;
                if (!profileService.save(profile)) {
                    std::cout << "Ошибка сохранения профиля\n";
                } else {
                    std::cout << "Профиль обновлен\n";
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
                std::cout << "Ошибка отправки\n";
            } else {
                std::cout << "Отправлено, id=" << *id << "\n";
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
            std::cout << "Найдено клиентов: " << peers.size() << "\n";
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
                std::cout << "Ошибка очистки кеша: " << ec.message() << "\n";
            } else {
                std::cout << "Кеш очищен. Удалено объектов: " << removedCount << "\n";
            }
            continue;
        }

        if (choice == "7") {
            std::cout << "Звонки, голосовые сообщения и передача файлов пока не поддерживаются в CLI/TUI.\n";
            std::cout << "Используйте внешний фронтенд через локальный API.\n";
            continue;
        }
    }

    return 0;
}

} // namespace zibby::cli
