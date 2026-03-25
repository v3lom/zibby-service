#include "core/service.h"

#include "api/api_server.h"
#include "core/crypto.h"
#include "core/database.h"
#include "core/message.h"
#include "core/network.h"
#include "core/profile.h"
#include "utils/logger.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace fs = boost::filesystem;

namespace zibby::core {

namespace {

std::string randomHexToken(std::size_t byteCount = 32) {
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(0, 255);

    std::ostringstream out;
    for (std::size_t index = 0; index < byteCount; ++index) {
        out << std::hex << std::setw(2) << std::setfill('0') << distribution(generator);
    }
    return out.str();
}

std::string loadOrCreateApiToken(const std::string& dataDir) {
    const fs::path tokenPath = fs::path(dataDir) / "api_token.txt";

    if (fs::exists(tokenPath)) {
        std::ifstream input(tokenPath.string());
        std::string token;
        std::getline(input, token);
        if (!token.empty()) {
            return token;
        }
    }

    const auto token = randomHexToken();
    std::ofstream output(tokenPath.string(), std::ios::trunc);
    output << token;
    return token;
}

} // namespace

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

    const std::string storageSecret = config_.dataDir + ":zibby-storage-secret";
    MessageService messageService(database, crypto, storageSecret);
    ProfileService profileService(database);
    profileService.ensureLocalProfile(boost::asio::ip::host_name());

    boost::asio::io_context ioContext;
    Network network(ioContext, config_.listenPort);
    network.start();

    const std::string apiToken = loadOrCreateApiToken(config_.dataDir);
    zibby::api::ApiServer apiServer(ioContext, database, messageService, profileService, config_.listenPort);
    const std::string apiEndpoint = "127.0.0.1:" + std::to_string(config_.apiPort);
    if (!apiServer.start(apiEndpoint, apiToken)) {
        zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Error, "API server start failed on " + apiEndpoint);
        return 4;
    }
    zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Info, "API server listening on " + apiEndpoint);

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
    apiServer.stop();
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

std::string Service::apiToken() const {
    const fs::path tokenPath = fs::path(config_.dataDir) / "api_token.txt";
    if (!fs::exists(tokenPath)) {
        return {};
    }

    std::ifstream input(tokenPath.string());
    std::string token;
    std::getline(input, token);
    return token;
}

std::string Service::apiEndpoint() const {
    return "127.0.0.1:" + std::to_string(config_.apiPort);
}

} // namespace zibby::core
