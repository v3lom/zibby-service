#pragma once

#include <boost/asio.hpp>
#include <array>
#include <string>
#include <vector>

#include "core/peer.h"

namespace zibby::core {

class Network {
public:
    Network(boost::asio::io_context& ioContext, int listenPort);
    void start();

    static std::vector<PeerInfo> discoverPeers(int listenPort, int timeoutMs = 1200);

private:
    void startUdpReceive();
    void startTcpAccept();
    std::string buildDiscoveryResponse() const;
    static PeerInfo parseDiscoveryResponse(const std::string& payload, const std::string& host);

    boost::asio::io_context& ioContext_;
    int listenPort_ = 0;
    boost::asio::ip::udp::socket udpSocket_;
    boost::asio::ip::udp::endpoint remoteEndpoint_;
    std::array<char, 1024> buffer_{};

    boost::asio::ip::tcp::acceptor tcpAcceptor_;
};

} // namespace zibby::core
