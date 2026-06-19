#pragma once
#include "Types/Type.hpp"

#include <boost/asio.hpp>
#include <string_view>
#include <thread>
#include "MakeStunRequest.hpp"

namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    class Client {
    public:
        Client(asio::io_context& io, std::string_view host, std::string_view port, std::string_view userName);

        void bindingRequest();
        asio::awaitable<void> listener();
        void getListConnectedUsers();
        void ConnectToClient(std::string_view clientName);
        asio::awaitable<void> sendMessage(std::vector<uint8_t> message);
        void menuLoop();

        void dispatchResponse(std::unique_ptr<StunMessage::StunMessageResponse> response);
        asio::any_io_executor get_executor();

    private:

        std::vector<Type::ConnectionUser> connectionList_;
        asio::io_context& io_;
        udp::resolver resolver_;
        udp::endpoint serverEndpoint_;
        udp::socket socket_;
        std::string userName_;
        bool serverConnected_ = false;
        bool startPrint_ = false;
        std::condition_variable cv_;
        std::mutex mutex_;
        std::jthread menuThread_;
    };
}
