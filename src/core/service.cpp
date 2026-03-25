#include "core/service.h"

#include "core/crypto.h"
#include "core/database.h"
#include "core/network.h"
#include "utils/logger.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <memory>
#include <string>

namespace fs = boost::filesystem;

namespace zibby::core {

Service::Service(Config config)
    : config_(std::move(config)) {}

int Service::run(bool daemonMode) {
    const fs::path logPath = fs::path(config_.dataDir) / config_.logFile;
    if (!zibby::utils::Logger::instance().initialize(logPath.string())) {
        return 1;
    }

    zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Info, "Starting zibby-service");
    if (daemonMode) {
        zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Info, "Daemon mode enabled");
    }

    Database database;
    if (!database.initialize(config_.dbFile)) {
        zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Error, "Database initialization failed");
        return 2;
    }

    Crypto crypto;
    if (!crypto.ensureUserKeys(config_.dataDir)) {
        zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Error, "Key generation failed");
        return 3;
    }

    boost::asio::io_context ioContext;
    Network network(ioContext, config_.listenPort);
    network.start();

    boost::asio::ip::tcp::acceptor controlAcceptor(
        ioContext,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), static_cast<unsigned short>(config_.controlPort)));

    std::function<void()> acceptControl;
    acceptControl = [&]() {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(ioContext);
        controlAcceptor.async_accept(*socket, [&, socket](const boost::system::error_code& error) {
            if (!error) {
                auto buffer = std::make_shared<std::array<char, 32>>();
                socket->async_read_some(
                    boost::asio::buffer(*buffer),
                    [socket, buffer](const boost::system::error_code& readError, std::size_t bytesRead) {
                        if (readError || bytesRead == 0) {
                            return;
                        }
                        const std::string command(buffer->data(), bytesRead);
                        const std::string response = (command.find("PING") == 0) ? "PONG\n" : "UNKNOWN\n";
                        boost::asio::async_write(*socket, boost::asio::buffer(response), [socket](const boost::system::error_code&, std::size_t) {});
                    });
            }
            acceptControl();
        });
    };

    acceptControl();

    boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Info, "Shutdown signal received");
        ioContext.stop();
    });

    ioContext.run();
    return 0;
}

bool Service::pingRunningInstance() const {
    try {
        boost::asio::io_context ioContext;
        boost::asio::ip::tcp::socket socket(ioContext);
        socket.connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), static_cast<unsigned short>(config_.controlPort)));

        const std::string request = "PING\n";
        boost::asio::write(socket, boost::asio::buffer(request));

        std::array<char, 32> response{};
        const auto bytes = socket.read_some(boost::asio::buffer(response));
        if (bytes == 0) {
            return false;
        }

        const std::string payload(response.data(), bytes);
        return payload.find("PONG") != std::string::npos;
    } catch (...) {
        return false;
    }
}

} // namespace zibby::core
