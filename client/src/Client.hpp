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
        Client(asio::io_context& io, std::string_view host, std::string_view port, std::string_view userName, int user_port);
        ~Client();

        asio::awaitable<void> listener();
        asio::awaitable<void> sendMessage(std::vector<uint8_t> message);
        void menuLoop();

        void dispatchResponse(std::unique_ptr<StunMessage::StunMessageResponse> response, const udp::endpoint& endpoint);

        asio::awaitable<void> initP2PConnection(uint16_t connectedPort, u_int32_t connectedTarget, StunMessage::Type role);

    private:
        void saveToken(std::string_view token);
        void loadToken();

        void MakeRequest(Type::Request::Attribute);

        void startConnection(uint16_t connectedPort, u_int32_t connectedTarget, StunMessage::Type role);

        void handleCommand(std::string_view line);

        void answerConsent(bool accept);

        std::vector<Type::ConnectionUser> connectionList_;
        asio::io_context& io_;
        udp::resolver resolver_;
        udp::endpoint serverEndpoint_;
        udp::socket socket_;
        asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
        uint16_t localPort_ = 0;

        std::string userName_;
        std::string token_;
        std::jthread menuThread_;
        std::atomic<AppState> appState_{AppState::Menu};
        std::string pendingIncomingUser_;

        MsQuicRegistration *reg_;
        MsQuicConfiguration *server_config_;
        MsQuicConfiguration *client_config_;

        std::unique_ptr<MsQuicListener> listener_;
        std::unique_ptr<P2P::Connection> connection_;
    };
}
