#pragma once
#include "Connection.hpp"
#include "Types/Type.hpp"
#include "MakeStunRequest.hpp"
#include "Util/AppState.hpp"

#include <boost/asio.hpp>
#include <string_view>
#include <thread>
#include <atomic>


namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    class Client {
    public:
        Client(asio::io_context& io, std::string_view host, std::string_view port, std::string_view userName);
        ~Client();

        void bindingRequest();
        asio::awaitable<void> listener();
        void getListConnectedUsers();
        void ConnectToClient(std::string_view clientName);
        asio::awaitable<void> sendMessage(std::vector<uint8_t> message);
        void menuLoop();

        void dispatchResponse(std::unique_ptr<StunMessage::StunMessageResponse> response, const udp::endpoint& endpoint);
        asio::any_io_executor get_executor();

        asio::awaitable<void> initP2PConnection(uint16_t connectedPort, u_int32_t connectedTarget, StunMessage::Type role);

    private:
        void saveToken(std::string_view token);
        void loadToken();

        void MakeRequest(Type::Request::Attribute);

        void startConnection(uint16_t connectedPort, u_int32_t connectedTarget, StunMessage::Type role);

        std::vector<Type::ConnectionUser> connectionList_;
        asio::io_context& io_;
        udp::resolver resolver_;
        udp::endpoint serverEndpoint_;
        udp::socket socket_;
        asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
        uint16_t localPort_ = 0;

        std::string userName_;
        bool serverConnected_ = false;
        bool startP2P_ = false;
        bool startPrint_ = false;
        std::string token_;
        std::condition_variable cv_;
        std::mutex mutex_;
        std::jthread menuThread_;
        std::atomic<AppState> appState_{AppState::Menu};

        MsQuicRegistration *reg_;
        MsQuicConfiguration *server_config_;
        MsQuicConfiguration *client_config_;

        std::unique_ptr<MsQuicListener> listener_;
        std::unique_ptr<P2P::Connection> connection_;
    };
}
