#pragma once

#include <boost/asio.hpp>

#include <array>

namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    class Server {
    public:
        Server(asio::io_context& io, short port);

    private:
        void handle_receive();
        void handle_send(std::size_t length);

        udp::socket socket_;
        udp::endpoint endpoint_;
        std::array<char, 1024> buffer_;
    };
}
