#include "api/api_server.h"

#include "version.h"
#include "api/api_protocol.h"
#include "core/database.h"
#include "core/message.h"
#include "core/network.h"
#include "core/peer.h"
#include "core/profile.h"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <sstream>

namespace zibby::api {

namespace pt = boost::property_tree;

struct ApiServer::Session {
    explicit Session(boost::asio::io_context& ioContext)
        : socket(ioContext) {}

    boost::asio::ip::tcp::socket socket;
    boost::asio::streambuf buffer;
    bool authenticated = false;
};

namespace {

std::string toJson(const pt::ptree& tree) {
    std::ostringstream output;
    pt::write_json(output, tree, false);
    return output.str();
}

pt::ptree parseJson(const std::string& text, bool* ok) {
    std::istringstream input(text);
    pt::ptree tree;
    try {
        pt::read_json(input, tree);
        *ok = true;
    } catch (...) {
        *ok = false;
    }
    return tree;
}

pt::ptree makeError(const std::string& id, const std::string& errorMessage) {
    pt::ptree response;
    response.put("id", id);
    response.put("ok", false);
    response.put("error", errorMessage);
    return response;
}

pt::ptree makeOk(const std::string& id) {
    pt::ptree response;
    response.put("id", id);
    response.put("ok", true);
    return response;
}

} // namespace

ApiServer::ApiServer(
    boost::asio::io_context& ioContext,
    zibby::core::Database& database,
    zibby::core::MessageService& messageService,
    zibby::core::ProfileService& profileService,
    int listenPort)
    : ioContext_(ioContext)
    , database_(database)
    , messageService_(messageService)
    , profileService_(profileService)
    , listenPort_(listenPort)
    , acceptor_(ioContext) {}

bool ApiServer::parseEndpoint(const std::string& endpoint, std::string* host, unsigned short* port) {
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

bool ApiServer::start(const std::string& endpoint, const std::string& authToken) {
    if (running_) {
        return true;
    }

    std::string host;
    unsigned short port = 0;
    if (!parseEndpoint(endpoint, &host, &port)) {
        return false;
    }

    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(host, ec);
    if (ec) {
        return false;
    }

    acceptor_.open(boost::asio::ip::tcp::v4(), ec);
    if (ec) {
        return false;
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        return false;
    }

    acceptor_.bind(boost::asio::ip::tcp::endpoint(address, port), ec);
    if (ec) {
        return false;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        return false;
    }

    authToken_ = authToken;
    running_ = true;
    startAccept();
    return true;
}

void ApiServer::stop() {
    if (!running_) {
        return;
    }

    boost::system::error_code ec;
    acceptor_.cancel(ec);
    acceptor_.close(ec);
    running_ = false;
}

bool ApiServer::running() const {
    return running_;
}

unsigned short ApiServer::listeningPort() const {
    if (!running_) {
        return 0;
    }
    return acceptor_.local_endpoint().port();
}

void ApiServer::startAccept() {
    if (!running_) {
        return;
    }

    auto session = std::make_shared<Session>(ioContext_);
    acceptor_.async_accept(session->socket, [this, session](const boost::system::error_code& ec) {
        if (!ec && running_) {
            startRead(session);
        }
        if (running_) {
            startAccept();
        }
    });
}

void ApiServer::startRead(const std::shared_ptr<Session>& session) {
    boost::asio::async_read_until(session->socket, session->buffer, '\n', [this, session](const boost::system::error_code& ec, std::size_t) {
        if (ec) {
            return;
        }

        std::istream input(&session->buffer);
        std::string line;
        std::getline(input, line);
        const auto response = handleRequest(session, line);
        writeResponse(session, response);
    });
}

void ApiServer::writeResponse(const std::shared_ptr<Session>& session, const std::string& responseJson) {
    auto payload = std::make_shared<std::string>(responseJson);
    if (payload->empty() || payload->back() != '\n') {
        payload->push_back('\n');
    }

    boost::asio::async_write(session->socket, boost::asio::buffer(*payload), [this, session, payload](const boost::system::error_code& ec, std::size_t) {
        if (!ec) {
            startRead(session);
        }
    });
}

std::string ApiServer::handleRequest(const std::shared_ptr<Session>& session, const std::string& requestLine) {
    bool jsonOk = false;
    const auto request = parseJson(requestLine, &jsonOk);
    if (!jsonOk) {
        return toJson(makeError("", "invalid_json"));
    }

    const std::string id = request.get<std::string>("id", "");
    const std::string method = request.get<std::string>("method", "");

    if (!session->authenticated) {
        if (method != "auth.login") {
            return toJson(makeError(id, "unauthorized"));
        }

        const std::string token = request.get<std::string>("params.token", "");
        if (token != authToken_) {
            return toJson(makeError(id, "invalid_token"));
        }

        session->authenticated = true;
        auto response = makeOk(id);
        response.put("result.authenticated", true);
        response.put("result.protocol", zibby::api::kProtocolVersion);
        return toJson(response);
    }

    try {
        if (method == "system.ping") {
            auto response = makeOk(id);
            response.put("result.pong", true);
            return toJson(response);
        }

        if (method == "profile.get") {
            const auto profile = profileService_.get().value_or(profileService_.ensureLocalProfile(boost::asio::ip::host_name()));
            auto response = makeOk(id);
            response.put("result.profile.id", profile.id);
            response.put("result.profile.display_name", profile.displayName);
            response.put("result.profile.avatar", profile.avatarPath);
            response.put("result.profile.bio", profile.bio);
            response.put("result.profile.birth_date", profile.birthDate);
            response.put("result.profile.status", profile.status);
            response.put("result.profile.public_key", profile.publicKey);
            return toJson(response);
        }

        if (method == "profile.update") {
            auto profile = profileService_.get().value_or(profileService_.ensureLocalProfile(boost::asio::ip::host_name()));
            if (const auto value = request.get_optional<std::string>("params.display_name")) {
                profile.displayName = *value;
            }
            if (const auto value = request.get_optional<std::string>("params.bio")) {
                profile.bio = *value;
            }
            if (const auto value = request.get_optional<std::string>("params.status")) {
                profile.status = *value;
            }
            if (!profileService_.save(profile)) {
                return toJson(makeError(id, "profile_update_failed"));
            }
            auto response = makeOk(id);
            response.put("result.updated", true);
            return toJson(response);
        }

        if (method == "message.send") {
            const auto messageId = messageService_.sendText(
                request.get<std::string>("params.chat", ""),
                request.get<std::string>("params.from", ""),
                request.get<std::string>("params.to", ""),
                request.get<std::string>("params.text", ""));
            if (!messageId.has_value()) {
                return toJson(makeError(id, "message_send_failed"));
            }
            auto response = makeOk(id);
            response.put("result.message_id", *messageId);
            return toJson(response);
        }

        if (method == "message.edit") {
            const auto ok = messageService_.editMessage(
                request.get<std::int64_t>("params.id", 0),
                request.get<std::string>("params.text", ""));
            if (!ok) {
                return toJson(makeError(id, "message_edit_failed"));
            }
            auto response = makeOk(id);
            response.put("result.updated", true);
            return toJson(response);
        }

        if (method == "message.read") {
            const auto ok = messageService_.markRead(request.get<std::int64_t>("params.id", 0));
            if (!ok) {
                return toJson(makeError(id, "message_read_failed"));
            }
            auto response = makeOk(id);
            response.put("result.updated", true);
            return toJson(response);
        }

        if (method == "message.history") {
            const auto records = messageService_.history(
                request.get<std::string>("params.chat", ""),
                static_cast<std::size_t>(request.get<int>("params.limit", 20)));
            auto response = makeOk(id);
            pt::ptree list;
            for (const auto& record : records) {
                pt::ptree item;
                item.put("id", record.id);
                item.put("chat", record.chatId);
                item.put("from", record.from);
                item.put("to", record.to);
                item.put("text", record.payload);
                item.put("read", record.read);
                item.put("edited", record.edited);
                item.put("created_at", record.createdAt);
                list.push_back(std::make_pair("", item));
            }
            response.add_child("result.messages", list);
            return toJson(response);
        }

        if (method == "peers.list") {
            const auto peers = database_.listPeers(static_cast<std::size_t>(request.get<int>("params.limit", 200)));
            auto response = makeOk(id);
            pt::ptree list;
            for (const auto& peer : peers) {
                pt::ptree item;
                item.put("peer_id", peer.peerId);
                item.put("name", peer.displayName);
                item.put("host", peer.host);
                item.put("port", peer.port);
                item.put("version", peer.version);
                item.put("last_seen", peer.lastSeen);
                list.push_back(std::make_pair("", item));
            }
            response.add_child("result.peers", list);
            return toJson(response);
        }

        if (method == "peers.discover") {
            const int timeoutMs = request.get<int>("params.timeout_ms", 1200);
            const auto discovered = zibby::core::Network::discoverPeers(listenPort_, timeoutMs);
            for (const auto& peer : discovered) {
                database_.upsertPeer(peer);
            }
            auto response = makeOk(id);
            response.put("result.count", static_cast<int>(discovered.size()));
            return toJson(response);
        }

        if (method == "peers.add") {
            zibby::core::PeerInfo peer;
            peer.host = request.get<std::string>("params.host", "");
            peer.port = request.get<int>("params.port", 0);
            peer.displayName = request.get<std::string>("params.name", "");
            peer.version = request.get<std::string>("params.version", "manual");
            peer.peerId = peer.host + ":" + std::to_string(peer.port);
            if (!database_.upsertPeer(peer)) {
                return toJson(makeError(id, "peer_add_failed"));
            }
            auto response = makeOk(id);
            response.put("result.saved", true);
            return toJson(response);
        }

        return toJson(makeError(id, "unknown_method"));
    } catch (...) {
        return toJson(makeError(id, "internal_error"));
    }
}

} // namespace zibby::api
