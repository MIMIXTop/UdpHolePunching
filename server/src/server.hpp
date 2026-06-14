#pragma once

#include <boost/asio.hpp>

#include <array>

#include "StunMessage.hpp"

namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    class Server {
    public:
        Server(asio::io_context& io, short port);

    private:
        void start_receive();
        void handle_send(std::shared_ptr<std::vector<uint8_t>>, const boost::system::error_code& ec, std::size_t bytes_transferred);
        void handle_receive( const boost::system::error_code& ec, std::size_t bytes_transferred);
        std::vector<uint8_t> handle_request(std::shared_ptr<std::vector<uint8_t>> data, std::shared_ptr<udp::endpoint> ep);

        std::vector<uint8_t> handle_binding_request(const StunMessage::Header &header, std::shared_ptr<udp::endpoint> ep);

        udp::socket socket_;
        udp::endpoint endpoint_;
        std::array<char, 1024> buffer_;
    };
}
