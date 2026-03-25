#include "api/api_server.h"
#include "core/crypto.h"
#include "core/database.h"
#include "core/message.h"
#include "core/profile.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

namespace {

std::string sendRequest(boost::asio::ip::tcp::socket& socket, const std::string& requestJson) {
    const std::string payload = requestJson + "\n";
    boost::asio::write(socket, boost::asio::buffer(payload));

    boost::asio::streambuf responseBuffer;
    boost::asio::read_until(socket, responseBuffer, '\n');
    std::istream input(&responseBuffer);
    std::string line;
    std::getline(input, line);
    return line;
}

boost::property_tree::ptree parseJson(const std::string& line) {
    boost::property_tree::ptree tree;
    std::istringstream in(line);
    boost::property_tree::read_json(in, tree);
    return tree;
}

} // namespace

int main() {
    namespace fs = boost::filesystem;

    const fs::path baseDir = fs::temp_directory_path() / fs::unique_path("zibby-api-test-%%%%-%%%%");
    fs::create_directories(baseDir);
    const fs::path dbPath = baseDir / "api.sqlite3";

    {
        zibby::core::Database database;
        if (!database.initialize(dbPath.string())) {
            std::cerr << "database init failed" << std::endl;
            return EXIT_FAILURE;
        }

        zibby::core::Crypto crypto;
        zibby::core::MessageService messageService(database, crypto, "api-test-secret");
        zibby::core::ProfileService profileService(database);
        profileService.ensureLocalProfile("api-tester");

        boost::asio::io_context ioContext;
        zibby::api::ApiServer apiServer(ioContext, database, messageService, profileService, 9876);
        if (!apiServer.start("127.0.0.1:0", "unit-token")) {
            std::cerr << "api start failed" << std::endl;
            return EXIT_FAILURE;
        }

        std::thread ioThread([&ioContext]() {
            ioContext.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        boost::asio::io_context clientContext;
        boost::asio::ip::tcp::socket socket(clientContext);
        socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), apiServer.listeningPort()));

        const auto unauthorized = parseJson(sendRequest(socket, R"({"id":"1","method":"system.ping"})"));
        if (unauthorized.get<std::string>("error", "") != "unauthorized") {
            std::cerr << "unauthorized check failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto auth = parseJson(sendRequest(socket, R"({"id":"2","method":"auth.login","params":{"token":"unit-token"}})"));
        if (!auth.get<bool>("ok", false)) {
            std::cerr << "auth failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto profile = parseJson(sendRequest(socket, R"({"id":"3","method":"profile.get"})"));
        if (!profile.get<bool>("ok", false)) {
            std::cerr << "profile.get failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto sent = parseJson(sendRequest(
            socket,
            R"({"id":"4","method":"message.send","params":{"chat":"api-chat","from":"alice","to":"bob","text":"hello"}})"));
        if (!sent.get<bool>("ok", false)) {
            std::cerr << "message.send failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto history = parseJson(sendRequest(
            socket,
            R"({"id":"5","method":"message.history","params":{"chat":"api-chat","limit":10}})"));
        if (!history.get<bool>("ok", false)) {
            std::cerr << "message.history failed" << std::endl;
            return EXIT_FAILURE;
        }

        auto messages = history.get_child_optional("result.messages");
        if (!messages.has_value() || messages->empty()) {
            std::cerr << "message.history empty" << std::endl;
            return EXIT_FAILURE;
        }

        const auto peerSaved = parseJson(sendRequest(
            socket,
            R"({"id":"6","method":"peers.add","params":{"host":"127.0.0.1","port":9876,"name":"manual"}})"));
        if (!peerSaved.get<bool>("ok", false)) {
            std::cerr << "peers.add failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto peerList = parseJson(sendRequest(socket, R"({"id":"7","method":"peers.list","params":{"limit":10}})"));
        if (!peerList.get<bool>("ok", false)) {
            std::cerr << "peers.list failed" << std::endl;
            return EXIT_FAILURE;
        }

        apiServer.stop();
        ioContext.stop();
        ioThread.join();
    }

    fs::remove_all(baseDir);
    return EXIT_SUCCESS;
}
