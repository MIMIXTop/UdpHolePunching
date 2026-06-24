#pragma once

#include <msquic.hpp>
#include <string_view>

namespace P2P {
    class Stream {
    public:
        Stream(const MsQuicConnection& conn);

        Stream(HQUIC native_stream);

        ~Stream() = default;

        void OnDataReceive(std::string_view data);
        void OnPeerShutdown();

        void Send(std::string& data, void* context = nullptr);

    private:
        static QUIC_STATUS QUIC_API CallbackHandle(MsQuicStream* stream, void* context, QUIC_STREAM_EVENT* event);

        MsQuicStream stream_;
    };
} // P2P