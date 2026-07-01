//
// Created by mimixtop on 19.06.2026.
//

#include "Stream.hpp"
#include "QuicNetLog.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <iostream>
#include <print>


namespace P2P {
    Stream::Stream(const MsQuicConnection &conn, const MsQuicConnection* parentConnection) :
     stream_(conn, QUIC_STREAM_OPEN_FLAG_NONE, CleanUpManual, CallbackHandle, this),
     parent_connection_(parentConnection == nullptr ? &conn : parentConnection) {
        std::println("Create stream native");
        stream_.Start(QUIC_STREAM_START_FLAG_IMMEDIATE);
    }

    Stream::Stream(HQUIC native_stream, const MsQuicConnection* parentConnection) :
    stream_(native_stream, CleanUpManual, CallbackHandle, this),
    parent_connection_(parentConnection) {
        std::println("Create stream native");
        stream_.Start();
    }

    void Stream::OnDataReceive(std::string_view data) {
        std::cout << "Получено: " << data << "\n";
    }

    void Stream::OnPeerShutdown() {
        std::println("Peer Shutdown 'stream'");
    }

    void Stream::Send(std::string& data, void *context) {
        auto* buffer = new QUIC_BUFFER();

        buffer->Length = data.size();
        buffer->Buffer = new uint8_t[data.size()];
        std::memcpy(buffer->Buffer, data.data(), data.size());

        stream_.Send(buffer, 1, QUIC_SEND_FLAG_NONE, buffer);
    }

    QUIC_STATUS QUIC_API Stream::CallbackHandle(MsQuicStream* stream, void *context, QUIC_STREAM_EVENT *event) {

        auto* self = static_cast<Stream*>(context);

        switch (event->Type) {
            case QUIC_STREAM_EVENT_SEND_COMPLETE: {
                auto* buffer = static_cast<QUIC_BUFFER *>(event->SEND_COMPLETE.ClientContext);
                delete buffer;
                break;
            }
            case QUIC_STREAM_EVENT_RECEIVE:
                for (int i = 0; i < event->RECEIVE.BufferCount; ++i) {
                    std::string_view recv_data(
                        reinterpret_cast<char*>(event->RECEIVE.Buffers[i].Buffer),
                        event->RECEIVE.Buffers[i].Length
                    );
                    self->OnDataReceive(recv_data);
                }
                break;
            case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
                self->OnPeerShutdown();
                break;
            case QUIC_STREAM_EVENT_CANCEL_ON_LOSS:
                QuicNetLog::Event("WARN", "stream", "cancel-on-loss", "send canceled due to packet loss");
                if (self->parent_connection_ != nullptr) {
                    QuicNetLog::PacketLoss("stream", "cancel-on-loss", *self->parent_connection_);
                }
                std::println("Packet loss error code: 0x{:x}", event->CANCEL_ON_LOSS.ErrorCode);
                break;
            default: break;
        }

        return QUIC_STATUS_SUCCESS;
    }
} // P2P