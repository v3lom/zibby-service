#include "core/network.h"

#include "utils/logger.h"

namespace zibby::core {

Network::Network(boost::asio::io_context& ioContext, int listenPort)
    : ioContext_(ioContext),
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
                if (payload == "ZIBBY_DISCOVER") {
                    static const std::string response = "ZIBBY_PONG";
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

} // namespace zibby::core
