#include "Connection.hpp"

#include <cstring>

#include "Stream.hpp"
#include "QuicNetLog.hpp"
#include <iostream>
#include <print>

namespace P2P {
    Connection::Connection(MsQuicRegistration& ms_reg) : connection_(ms_reg, CleanUpManual, ClientCallback, this) {
        std::println("Create Connection [Client mode]");
    }

    Connection::Connection(HQUIC connection) : connection_(connection, CleanUpManual, ServerCallback, this) {
        std::println("Create Connection [Server mode]");
    }

    void Connection::onClientConnected() {
        std::println("[Client] Успешно подключились к серверу! Создаем стрим для чата...");
        QuicNetLog::Event("INFO", "client", "connected", "peer connection established");
        stream_ = std::make_unique<Stream>(connection_, &connection_);
    }

    void Connection::onServerConnected() {
        std::println("[Server] Клиент подключился! Ожидаем открытия стрима");
        QuicNetLog::Event("INFO", "server", "connected", "incoming peer connection established");
    }

    void Connection::onDisconnected() {
        std::cout << "Соединение разорвано собеседником.\n";
    }

    void Connection::onServerIncomingStream(HQUIC stream) {
        std::println("Друг открыл стрим. Создаем чат-сессию");
        stream_ = std::make_unique<Stream>(stream, &connection_);
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

    QUIC_STATUS QUIC_API Connection::ClientCallback(MsQuicConnection *connection, void *context, QUIC_CONNECTION_EVENT *event) {
        auto* self = static_cast<Connection*>(context);
        switch (event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                self->onClientConnected();
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                std::println("[Client] SHUTDOWN_INITIATED_BY_PEER: {}", event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
                QuicNetLog::Event("WARN", "client", "shutdown-by-peer", "peer initiated connection shutdown");
                self->onDisconnected();
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::println("[Client MsQuic] Соединение полностью закрыто.");
                QuicNetLog::PacketLoss("client", "shutdown-complete", self->connection_);
                break;
            case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED:
                return QUIC_STATUS_SUCCESS;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
                    std::println("[Client] Закрыто по таймауту (Idle)");
                    QuicNetLog::Event("WARN", "client", "transport-shutdown", "idle timeout");
                } else {
                    std::println("[Client MsQuic] Ошибка рукопожатия (Транспорт): 0x{:x}", event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
                    QuicNetLog::Event("ERROR", "client", "transport-shutdown", "transport failure");
                }
                QuicNetLog::PacketLoss("client", "transport-shutdown", self->connection_);
                break;
            default: break;
        }
        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS QUIC_API Connection::ServerCallback(MsQuicConnection *connection, void *context, QUIC_CONNECTION_EVENT *event) {
        auto* self = static_cast<Connection*>(context);
        switch (event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                self->onServerConnected();
                break;
            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                self->onServerIncomingStream(event->PEER_STREAM_STARTED.Stream);
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                std::println("[Server] SHUTDOWN_INITIATED_BY_PEER: {}", event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
                QuicNetLog::Event("WARN", "server", "shutdown-by-peer", "peer initiated connection shutdown");
                self->onDisconnected();
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::println("[Server MsQuic] Соединение полностью закрыто.");
                QuicNetLog::PacketLoss("server", "shutdown-complete", self->connection_);
                break;
            case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED:
                return QUIC_STATUS_SUCCESS;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
                    std::println("[Server] Закрыто по таймауту (Idle)");
                    QuicNetLog::Event("WARN", "server", "transport-shutdown", "idle timeout");
                } else {
                    std::println("[Server MsQuic] Ошибка (Транспорт): 0x{:x}", event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
                    QuicNetLog::Event("ERROR", "server", "transport-shutdown", "transport failure");
                }
                QuicNetLog::PacketLoss("server", "transport-shutdown", self->connection_);
                break;
            default: break;
        }
        return QUIC_STATUS_SUCCESS;
    }
} // P2P