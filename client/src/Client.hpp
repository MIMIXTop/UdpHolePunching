#pragma once
#include "Types/Type.hpp"

#include <boost/asio.hpp>
#include <string_view>

namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    class Client {
    public:
        Client(asio::io_context& io, std::string_view host, std::string_view port, std::string_view userName);

        asio::awaitable<void> bindingRequest();
        asio::awaitable<void> listener();
        asio::awaitable<void> getListConnectedUsers();
        asio::awaitable<void> CoonnectToClient(std::string_view clientName);
        asio::awaitable<std::vector<uint8_t>> sendMessage(std::span<uint8_t> message);
        asio::any_io_executor get_executor();
    private:
        std::vector<Type::ConnectionUser> connectionList_;
        udp::resolver resolver_;
        udp::endpoint endpoint_;
        udp::socket socket_;
        std::string userName_;
        bool serverConnected_ = false;
    };
}