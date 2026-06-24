#include "Connection.hpp"

#include <cstring>

#include "Stream.hpp"
#include <iostream>
#include <print>

namespace P2P {
    Connection::Connection(MsQuicRegistration& ms_reg) : connection_(ms_reg, CleanUpManual, Callback, this) {
        //connection_.SetShareUdpBinding(true);
        std::println("Create connection C++ api");
    }

    Connection::Connection(HQUIC connection) : connection_(connection, CleanUpManual, Callback, this) {
        //connection_.SetShareUdpBinding(true);
        std::println("Create connection native api");
    }

    void Connection::OnConnected() {
        std::cout << "Мы подключились к другу! Можем общаться.\n";
        stream_ = std::make_unique<Stream>(connection_);
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
        if (stream_) {
            stream_->Send(text);
        } else {
            std::println("Stream is not create");
        }
    }

    QUIC_STATUS QUIC_API Connection::Callback(MsQuicConnection *connection, void *context, QUIC_CONNECTION_EVENT* event) {
        auto* self = static_cast<Connection*>(context);
        switch (event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED: {
                self->OnConnected();
                //std::string message = "Привет, это зашифрованное сообщение по QUIC!";
                //self->stream_->Send(message);
                break;
            }
            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                self->OnIncomingStream(event->PEER_STREAM_STARTED.Stream);
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                std::println("SHUTDOWN_INITIATED_BY_PEER: {}",event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
                self->OnDisconnected();
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::println("[MsQuic] Соединение полностью закрыто.");
                break;
            case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED:
                return QUIC_STATUS_SUCCESS;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
                    std::println("Successfully shut down on idle");
                } else {
                    std::println( "[MsQuic] Ошибка рукопожатия (Транспорт): 0x{:x}", event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
                }
                break;
        }
        return QUIC_STATUS_SUCCESS;
    }
} // P2P