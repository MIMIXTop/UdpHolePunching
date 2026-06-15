#pragma once

#include <boost/asio.hpp>

#include <array>

#include "Types/StunMessage.hpp"
#include "Types/ConnectedClient.hpp"

namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    class Server {
    public:
        Server(asio::io_context& io, short port);

    private:
        asio::awaitable<void> listen();

        asio::awaitable<void> worker(std::span<uint8_t> data, std::shared_ptr<udp::endpoint> endpoint);

        StunMessage::StunMessageRequest parse_raw_message(std::span<uint8_t> data);

        void start_receive();
        void handle_send(std::shared_ptr<std::vector<uint8_t>>, const boost::system::error_code& ec, std::size_t bytes_transferred);
        void handle_receive( const boost::system::error_code& ec, std::size_t bytes_transferred);
        std::vector<uint8_t> handle_request(std::shared_ptr<std::vector<uint8_t>> data, std::shared_ptr<udp::endpoint> ep);

        std::vector<uint8_t> handle_binding_request(StunMessage::StunMessageRequest message, std::shared_ptr<udp::endpoint> client_endpoint);

        std::vector<uint8_t> handle_binding_request(const StunMessage::Header &header, std::shared_ptr<udp::endpoint> ep);

        std::vector<Type::ConnectedClient> connected_clients {};
        int port_;
        udp::socket socket_;
        udp::endpoint endpoint_;
        std::array<char, 1024> buffer_;
    };
}
