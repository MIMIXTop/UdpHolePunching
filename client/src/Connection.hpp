#pragma once

#include "Stream.hpp"

#include <msquic.hpp>
#include <memory>

namespace P2P {
    class Connection {
    public:
        Connection(MsQuicRegistration& ms_reg);
        Connection(HQUIC connection);
        ~Connection() = default;

        void onClientConnected();
        void onServerConnected();
        void onDisconnected();
        void onServerIncomingStream(HQUIC stream);

        MsQuicConnection& get_connection();

        void SendMessage(std::string &text);
    private:
        MsQuicConnection connection_;
        std::unique_ptr<Stream> stream_;
        
        static QUIC_STATUS QUIC_API ClientCallback(MsQuicConnection* connection, void* context, QUIC_CONNECTION_EVENT* event);
        static QUIC_STATUS QUIC_API ServerCallback(MsQuicConnection* connection, void* context, QUIC_CONNECTION_EVENT* event);
    };
} // P2P
