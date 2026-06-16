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

        StunMessage::StunMessageRequest parseRawMessage(std::span<uint8_t> data);
        std::vector<uint8_t> parseStunMessageToRaw(const StunMessage::StunMessageResponse& response);

        std::vector<uint8_t> handleBindingRequest(const StunMessage::StunMessageRequest& message, std::shared_ptr<udp::endpoint> client_endpoint);

        std::vector<uint8_t> handleBindingRequest(const StunMessage::Header &header, std::shared_ptr<udp::endpoint> ep);

        std::vector<Type::ConnectedClient> connected_clients {};
        int port_;
        udp::socket socket_;
        udp::endpoint endpoint_;
        std::array<char, 1024> buffer_;
    };
}
