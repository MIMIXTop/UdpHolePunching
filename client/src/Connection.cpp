#include "Connection.hpp"
#include "Stream.hpp"
#include <iostream>
#include <print>

namespace P2P {
    Connection::Connection(MsQuicRegistration& ms_reg) : connection_(ms_reg, CleanUpManual, Callback, this) {
        std::println("Create connection C++ api");
    }

    Connection::Connection(HQUIC connection) : connection_(connection, CleanUpManual, Callback, this) {
        std::println("Create connection native api");
    }

    void Connection::OnConnected() {
        std::cout << "Мы подключились к другу! Можем общаться.\n";
    }

    void Connection::OnDisconnected() {
        std::cout << "Соединение разорвано собеседником.\n";
    }

    void Connection::OnIncomingStream(HQUIC stream) {
        std::cout << "Друг открыл стрим. Создаем чат-сессию...\n";
        stream_ = std::make_unique<Stream>(stream);
    }

    MsQuicConnection & Connection::get_connection() {
        return connection_;
    }

    void Connection::SendMessage(std::string &text) {
        stream_ = std::make_unique<Stream>(connection_);
        stream_->Send(text);
    }

    QUIC_STATUS QUIC_API Connection::Callback(MsQuicConnection *connection, void *context, QUIC_CONNECTION_EVENT* event) {
        auto* self = static_cast<Connection*>(context);
        switch (event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                self->OnConnected();
                break;
            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                self->OnIncomingStream(event->PEER_STREAM_STARTED.Stream);
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                self->OnDisconnected();
                break;
        }
        return QUIC_STATUS_SUCCESS;
    }
} // P2P