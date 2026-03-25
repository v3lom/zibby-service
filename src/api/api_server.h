#pragma once

#include <boost/asio.hpp>

#include <memory>
#include <string>

namespace zibby::core {
class Database;
class MessageService;
class ProfileService;
}

namespace zibby::api {

/**
 * @brief Локальный API сервер для фронтенд-клиентов.
 *
 * Протокол: JSON over TCP (один JSON-объект на строку).
 */
class ApiServer {
public:
    /**
     * @brief Создаёт API сервер.
     * @param ioContext Контекст ввода-вывода.
     * @param database База данных сервиса.
     * @param messageService Сервис сообщений.
     * @param profileService Сервис профиля.
     * @param listenPort Порт P2P сервиса (используется для discovery).
     */
    ApiServer(
        boost::asio::io_context& ioContext,
        zibby::core::Database& database,
        zibby::core::MessageService& messageService,
        zibby::core::ProfileService& profileService,
        int listenPort);

    /**
     * @brief Запускает API-сервер на endpoint.
     * @param endpoint Строка вида host:port (например 127.0.0.1:9888).
     * @param authToken Токен для auth.login.
     * @return true при успешном запуске.
     */
    bool start(const std::string& endpoint, const std::string& authToken);

    /**
     * @brief Останавливает API сервер.
     */
    void stop();

    /**
     * @brief Признак запущенного состояния.
     */
    bool running() const;

    /**
     * @brief Реальный порт после bind/listen.
     */
    unsigned short listeningPort() const;

private:
    struct Session;

    void startAccept();
    void startRead(const std::shared_ptr<Session>& session);
    void writeResponse(const std::shared_ptr<Session>& session, const std::string& responseJson);
    std::string handleRequest(const std::shared_ptr<Session>& session, const std::string& requestLine);

    static bool parseEndpoint(const std::string& endpoint, std::string* host, unsigned short* port);

    boost::asio::io_context& ioContext_;
    zibby::core::Database& database_;
    zibby::core::MessageService& messageService_;
    zibby::core::ProfileService& profileService_;
    int listenPort_ = 0;

    boost::asio::ip::tcp::acceptor acceptor_;
    std::string authToken_;
    bool running_ = false;
};

} // namespace zibby::api
