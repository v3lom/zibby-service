#include "cli/cli.h"

#include "utils/ansi.h"

#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cctype>
#include <iostream>
#include <sstream>

namespace {

namespace pt = boost::property_tree;

std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])) != 0) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(start, end - start);
}

bool parseEndpoint(const std::string& endpoint, std::string* host, unsigned short* port) {
    const auto delimiter = endpoint.rfind(':');
    if (delimiter == std::string::npos || delimiter == endpoint.size() - 1) {
        return false;
    }

    *host = endpoint.substr(0, delimiter);
    try {
        const auto parsed = std::stoul(endpoint.substr(delimiter + 1));
        if (parsed > 65535) {
            return false;
        }
        *port = static_cast<unsigned short>(parsed);
    } catch (...) {
        return false;
    }

    return !host->empty();
}

std::string toJsonLine(const pt::ptree& tree) {
    std::ostringstream out;
    pt::write_json(out, tree, false);
    std::string s = out.str();
    s = trim(s);
    s.push_back('\n');
    return s;
}

pt::ptree parseJson(const std::string& line, bool* ok) {
    pt::ptree tree;
    try {
        std::istringstream in(line);
        pt::read_json(in, tree);
        *ok = true;
    } catch (...) {
        *ok = false;
    }
    return tree;
}

pt::ptree makeRequest(const std::string& id, const std::string& method) {
    pt::ptree req;
    req.put("id", id);
    req.put("method", method);
    return req;
}

std::string readLine(boost::asio::ip::tcp::socket& socket) {
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, '\n');
    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    return line;
}

bool rpc(boost::asio::ip::tcp::socket& socket, const pt::ptree& request, pt::ptree* response, std::string* rawResponse) {
    const std::string wire = toJsonLine(request);
    boost::asio::write(socket, boost::asio::buffer(wire));

    const std::string line = readLine(socket);
    if (rawResponse) {
        *rawResponse = line;
    }

    bool ok = false;
    const auto parsed = parseJson(line, &ok);
    if (!ok) {
        return false;
    }
    if (response) {
        *response = parsed;
    }
    return true;
}

void printHelp() {
    const auto a = zibby::utils::Ansi::forStdout();
    std::cout << a.bold() << "Commands:" << a.reset() << "\n";
    std::cout << "  " << a.cyan() << "help" << a.reset() << "\n";
    std::cout << "  " << a.cyan() << "api-info" << a.reset() << "\n";
    std::cout << "  " << a.cyan() << "ping" << a.reset() << "\n";
    std::cout << "  " << a.cyan() << "profile-get" << a.reset() << "\n";
    std::cout << "  " << a.cyan() << "profile-set-name" << a.reset() << " <name>\n";
    std::cout << "  " << a.cyan() << "peers" << a.reset() << "\n";
    std::cout << "  " << a.cyan() << "quit" << a.reset() << " | " << a.cyan() << "exit" << a.reset() << "\n";
}

} // namespace

namespace zibby::cli {

int Cli::run() {
    return 0;
}

int Cli::run(const std::string& apiEndpoint, const std::string& apiToken) {
    const auto out = zibby::utils::Ansi::forStdout();
    const auto err = zibby::utils::Ansi::forStderr();

    if (apiEndpoint.empty()) {
        std::cerr << err.red() << "CLI error: " << err.reset() << "empty api endpoint" << '\n';
        return 2;
    }

    if (apiToken.empty()) {
        std::cerr << err.red() << "CLI error: " << err.reset()
                  << "api token is empty (start the service once to generate token)" << '\n';
        return 2;
    }

    std::string host;
    unsigned short port = 0;
    if (!parseEndpoint(apiEndpoint, &host, &port)) {
        std::cerr << err.red() << "CLI error: " << err.reset()
                  << "invalid api endpoint format, expected host:port" << '\n';
        return 2;
    }

    try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        boost::asio::ip::tcp::socket socket(io);

        const auto endpoints = resolver.resolve(host, std::to_string(port));
        boost::asio::connect(socket, endpoints);

        // auth.login
        {
            auto req = makeRequest("1", "auth.login");
            pt::ptree params;
            params.put("token", apiToken);
            req.add_child("params", params);

            pt::ptree resp;
            std::string raw;
            if (!rpc(socket, req, &resp, &raw)) {
                std::cerr << err.red() << "CLI error: " << err.reset() << "invalid auth response: " << raw << '\n';
                return 3;
            }

            const bool ok = resp.get<bool>("ok", false);
            if (!ok) {
                std::cerr << err.red() << "CLI auth failed: " << err.reset()
                          << resp.get<std::string>("error", "unknown") << '\n';
                return 3;
            }
        }

        std::cout << out.green() << "Connected" << out.reset() << " to API at " << apiEndpoint << "\n";
        std::cout << "Type '" << out.cyan() << "help" << out.reset() << "' for commands.\n";

        std::uint64_t requestId = 2;
        std::string line;
        while (true) {
            std::cout << out.cyan() << "zibby" << out.reset() << "> ";
            if (!std::getline(std::cin, line)) {
                break;
            }

            line = trim(line);
            if (line.empty()) {
                continue;
            }

            if (line == "quit" || line == "exit") {
                break;
            }

            if (line == "help") {
                printHelp();
                continue;
            }

            if (line == "api-info") {
                std::cout << "api_endpoint=" << apiEndpoint << '\n';
                std::cout << "api_token=" << apiToken << '\n';
                continue;
            }

            pt::ptree req;
            if (line == "ping") {
                req = makeRequest(std::to_string(requestId++), "system.ping");
            } else if (line == "profile-get") {
                req = makeRequest(std::to_string(requestId++), "profile.get");
            } else if (line.rfind("profile-set-name ", 0) == 0) {
                req = makeRequest(std::to_string(requestId++), "profile.update");
                pt::ptree params;
                params.put("display_name", trim(line.substr(std::string("profile-set-name ").size())));
                req.add_child("params", params);
            } else if (line == "peers") {
                req = makeRequest(std::to_string(requestId++), "peers.list");
            } else {
                std::cerr << err.yellow() << "Unknown command." << err.reset() << " Type 'help'." << '\n';
                continue;
            }

            std::string raw;
            if (!rpc(socket, req, nullptr, &raw)) {
                std::cerr << err.red() << "CLI error: " << err.reset() << "invalid response" << '\n';
                continue;
            }
            std::cout << raw << '\n';
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << err.red() << "CLI error: " << err.reset() << ex.what() << '\n';
        return 3;
    }
}

} // namespace zibby::cli
