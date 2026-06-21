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

        void OnConnected();
        void OnDisconnected();
        void OnIncomingStream(HQUIC stream);

        MsQuicConnection& get_connection();

        void SendMessage(std::string &text);
    private:
        MsQuicConnection connection_;
        std::unique_ptr<Stream> stream_;

        static QUIC_STATUS QUIC_API Callback(MsQuicConnection* connection, void* context, QUIC_CONNECTION_EVENT* event);
    };
} // P2P
