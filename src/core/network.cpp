#include "core/network.h"

#include "utils/logger.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/host_name.hpp>

#include <chrono>
#include <set>
#include <sstream>

namespace zibby::core {

Network::Network(boost::asio::io_context& ioContext, int listenPort)
    : ioContext_(ioContext),
    listenPort_(listenPort),
      udpSocket_(ioContext, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), static_cast<unsigned short>(listenPort))),
      tcpAcceptor_(ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), static_cast<unsigned short>(listenPort))) {}

void Network::start() {
    startUdpReceive();
    startTcpAccept();
}

void Network::startUdpReceive() {
    udpSocket_.async_receive_from(
        boost::asio::buffer(buffer_),
        remoteEndpoint_,
        [this](const boost::system::error_code& error, std::size_t bytesReceived) {
            if (!error && bytesReceived > 0) {
                const std::string payload(buffer_.data(), bytesReceived);
                if (payload.rfind("ZIBBY_DISCOVER", 0) == 0) {
                    const std::string response = buildDiscoveryResponse();
                    udpSocket_.async_send_to(
                        boost::asio::buffer(response),
                        remoteEndpoint_,
                        [](const boost::system::error_code&, std::size_t) {});
                }
            }
            startUdpReceive();
        });
}

void Network::startTcpAccept() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(ioContext_);
    tcpAcceptor_.async_accept(
        *socket,
        [this, socket](const boost::system::error_code& error) {
            if (!error) {
                zibby::utils::Logger::instance().log(zibby::utils::LogLevel::Info, "Incoming TCP connection accepted");
            }
            startTcpAccept();
        });
}

std::string Network::buildDiscoveryResponse() const {
    std::ostringstream out;
    out << "ZIBBY_PONG|"
        << boost::asio::ip::host_name() << "|"
        << listenPort_ << "|"
        << ZIBBY_VERSION;
    return out.str();
}

PeerInfo Network::parseDiscoveryResponse(const std::string& payload, const std::string& host) {
    PeerInfo peer;
    if (payload.rfind("ZIBBY_PONG|", 0) != 0) {
        return peer;
    }

    std::stringstream ss(payload);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(ss, token, '|')) {
        parts.push_back(token);
    }

    if (parts.size() < 4) {
        return peer;
    }

    peer.peerId = host + ":" + parts[2];
    peer.displayName = parts[1];
    peer.host = host;
    try {
        peer.port = std::stoi(parts[2]);
    } catch (...) {
        peer.port = 0;
    }
    peer.version = parts[3];
    return peer;
}

std::vector<PeerInfo> Network::discoverPeers(int listenPort, int timeoutMs) {
    boost::asio::io_context ioContext;
    boost::asio::ip::udp::socket socket(ioContext);
    socket.open(boost::asio::ip::udp::v4());
    socket.set_option(boost::asio::socket_base::broadcast(true));
    socket.set_option(boost::asio::socket_base::reuse_address(true));
    socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));

    const std::string request = "ZIBBY_DISCOVER|cli";
    socket.send_to(
        boost::asio::buffer(request),
        boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), static_cast<unsigned short>(listenPort)));

    std::vector<PeerInfo> peers;
    std::set<std::string> seen;

    boost::asio::steady_timer timer(ioContext);
    timer.expires_after(std::chrono::milliseconds(timeoutMs));
    timer.async_wait([&](const boost::system::error_code&) {
        socket.cancel();
    });

    std::array<char, 1024> receiveBuffer{};
    boost::asio::ip::udp::endpoint from;

    std::function<void()> receiveLoop;
    receiveLoop = [&]() {
        socket.async_receive_from(boost::asio::buffer(receiveBuffer), from, [&](const boost::system::error_code& ec, std::size_t bytes) {
            if (!ec && bytes > 0) {
                const std::string payload(receiveBuffer.data(), bytes);
                const auto candidate = parseDiscoveryResponse(payload, from.address().to_string());
                if (!candidate.peerId.empty() && candidate.port > 0 && seen.insert(candidate.peerId).second) {
                    peers.push_back(candidate);
                }
                receiveLoop();
            }
        });
    };

    receiveLoop();
    ioContext.run();
    return peers;
}

} // namespace zibby::core
