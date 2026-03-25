#pragma once

#include <boost/asio.hpp>
#include <array>

namespace zibby::core {

class Network {
public:
    Network(boost::asio::io_context& ioContext, int listenPort);
    void start();

private:
    void startUdpReceive();
    void startTcpAccept();

    boost::asio::io_context& ioContext_;
    boost::asio::ip::udp::socket udpSocket_;
    boost::asio::ip::udp::endpoint remoteEndpoint_;
    std::array<char, 1024> buffer_{};

    boost::asio::ip::tcp::acceptor tcpAcceptor_;
};

} // namespace zibby::core
