#pragma once

#include <optional>
#include <string>

namespace zibby::core {

class Database;

/**
 * @brief Профиль пользователя сервиса zibby-service.
 */
struct UserProfile {
    std::string id = "local";
    std::string displayName;
    std::string avatarPath;
    std::string bio;
    std::string birthDate;
    std::string status = "active";
    std::string publicKey;
};

/**
 * @brief Сервис управления локальным профилем пользователя.
 */
class ProfileService {
public:
    /**
     * @brief Создаёт сервис профиля.
     * @param database Инициализированная БД сервиса.
     */
    explicit ProfileService(Database& database);

    /**
     * @brief Возвращает профиль или создаёт профиль по умолчанию.
     * @param defaultName Имя по умолчанию (hostname/username).
     * @return Профиль пользователя.
     */
    UserProfile ensureLocalProfile(const std::string& defaultName);

    /**
     * @brief Загружает профиль по идентификатору.
     * @param id Идентификатор профиля.
     * @return Профиль, если найден.
     */
    std::optional<UserProfile> get(const std::string& id = "local") const;

    /**
     * @brief Сохраняет изменения профиля.
     * @param profile Профиль для сохранения.
     * @return true при успешном сохранении.
     */
    bool save(const UserProfile& profile) const;

    /**
     * @brief Проверяет валидность отображаемого имени.
     * @param name Имя пользователя.
     * @return true если имя соответствует ограничениям.
     */
    static bool isValidDisplayName(const std::string& name);

private:
    Database& database_;
};

} // namespace zibby::core
