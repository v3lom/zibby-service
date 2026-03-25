#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zibby::core {

class Database;
class Crypto;

/**
 * @brief Базовая модель сообщения в сервисе.
 */
struct Message {
    std::int64_t id = 0;
    std::string chatId;
    std::string from;
    std::string to;
    std::string payload;
    bool read = false;
    bool edited = false;
    std::string createdAt;
    std::string editedAt;
    std::string readAt;
};

/**
 * @brief Сервис прикладных операций с сообщениями.
 *
 * Потокобезопасность: класс сам не хранит shared-state кроме ссылок на потокобезопасные зависимости.
 */
class MessageService {
public:
    /**
     * @brief Создаёт сервис сообщений.
     * @param database Экземпляр базы данных.
     * @param crypto Экземпляр криптографии.
     * @param storageSecret Секрет для шифрования payload в базе.
     */
    MessageService(Database& database, const Crypto& crypto, std::string storageSecret);

    /**
     * @brief Отправляет текстовое сообщение и сохраняет его в БД.
     * @param chatId Идентификатор чата.
     * @param from Идентификатор отправителя.
     * @param to Идентификатор получателя/канала.
     * @param text Текст сообщения.
     * @return ID сообщения при успехе, std::nullopt при ошибке.
     */
    std::optional<std::int64_t> sendText(const std::string& chatId, const std::string& from, const std::string& to, const std::string& text);

    /**
     * @brief Редактирует текст ранее отправленного сообщения.
     * @param messageId ID сообщения.
     * @param newText Новый текст сообщения.
     * @return true если запись обновлена.
     */
    bool editMessage(std::int64_t messageId, const std::string& newText);

    /**
     * @brief Отмечает сообщение как прочитанное.
     * @param messageId ID сообщения.
     * @return true если запись обновлена.
     */
    bool markRead(std::int64_t messageId);

    /**
     * @brief Возвращает историю чата с расшифрованным payload.
     * @param chatId Идентификатор чата.
     * @param limit Максимум сообщений.
     * @return Список сообщений в порядке создания.
     */
    std::vector<Message> history(const std::string& chatId, std::size_t limit = 100) const;

private:
    static bool validateText(const std::string& text);

    Database& database_;
    const Crypto& crypto_;
    std::string storageSecret_;
};

} // namespace zibby::core
