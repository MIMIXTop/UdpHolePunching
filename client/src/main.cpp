#include "MakeStunRequest.hpp"

#include "Client.hpp"

#include <iostream>
#include <print>
#include <boost/asio.hpp>

const MsQuicApi* MsQuic = new MsQuicApi();

int main() {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    if (QUIC_FAILED(MsQuic->GetInitStatus())) {
        std::println("FATAL ERROR: Failed to initialize MsQuic!");
        return -1;
    }

    asio::io_context context{8};

    Network::Client client(context, "192.168.1.9", "12345", "UserA");

    //Network::Client client(context, "192.168.1.9" , "12345", "UserB");

    context.run();
    // try {
    //     asio::io_context io;
    //     udp::resolver resolver(io);
    //     udp::endpoint endpoint = *resolver.resolve(udp::v4(), "127.0.0.1", "12345").begin();
    //     udp::socket socket(io);
    //     socket.open(udp::v4());
    //
    //     std::cout << "Sending Binding Request to stun.l.google.com:19302...\n";
    //     std::array<uint8_t, 20> message{};
    //     uint16_t message_type = 0x0001;
    //     uint16_t message_length = 0x0000;
    //     uint32_t cookie = 0x2112A442;
    //     std::vector<uint8_t> tx_id = Network::make_transaction_identifier();
    //
    //     message[0] = (message_type >> 8) & 0xff;
    //     message[1] = message_type & 0xff;
    //     message[2] = (message_length >> 8) & 0xff;
    //     message[3] = message_length & 0xff;
    //     message[4] = (cookie >> 24) & 0xff;
    //     message[5] = (cookie >> 16) & 0xff;
    //     message[6] = (cookie >> 8) & 0xff;
    //     message[7] = cookie & 0xff;
    //     for (int i = 0; i < 12; i++) {
    //         message[8 + i] = tx_id[11 - i];
    //     }
    //
    //     socket.send_to(asio::buffer(message), endpoint);
    //
    //     std::array<uint8_t, 1024> recv_buf{};
    //     udp::endpoint sender_endpoint;
    //     size_t len = socket.receive_from(asio::buffer(recv_buf), sender_endpoint);
    //
    //     Network::Types::StunMessageHeader responseHeader{};
    //     std::span<uint8_t> msg_type{recv_buf.begin(), recv_buf.begin() + 2};
    //     responseHeader.message_type = recv_buf[0] << 8 | recv_buf[1];
    //     switch (responseHeader.message_type) {
    //         case 0x0101:
    //             std::cout << "Stun Message Types Success " << "\n";
    //             break;
    //         case 0x0111:
    //             std::cout << "Stun Message Types Failure " << "\n";
    //            return 1;
    //         default:
    //             std::cout << "Stun Message Types Unknown Error " << "\n";
    //             return -1;
    //     }
    //
    //     std::vector<uint8_t> resAttr(len - 20);
    //     std::copy(recv_buf.begin() + 20, recv_buf.end(), resAttr.begin());
    //
    //     uint16_t resType = resAttr[0] << 8 | resAttr[1];
    //     if (resType != 0x0020) {
    //         std::cout << "Stun Message Types Unknown Error " << "\n";
    //         return -1;
    //     }
    //
    //     uint16_t resLength = resAttr[2] << 8 | resAttr[3];
    //
    //     if (resLength <= 0) {
    //         std::cout << "Stun Message Length Unknown Error " << "\n";
    //         return -1;
    //     }
    //
    //     uint8_t ipType = resAttr[5];
    //
    //     if (ipType != 0x01) {
    //         std::cout << "Stun Message Types Unknown Error " << "\n";
    //         return -1;
    //     }
    //
    //     uint16_t port = resAttr[6] << 8 | resAttr[7];
    //     uint32_t host = resAttr[8] << 24 | resAttr[9] << 16 | resAttr[10] << 8 | resAttr[11];
    //
    //
    //     uint16_t port_dec = port ^ (cookie >> 16);
    //     uint32_t host_dec = host ^ cookie;
    //     std::array<uint8_t, 4> host_dec_arr{};
    //     host_dec_arr[0] = (host_dec >> 24) & 0xff;
    //     host_dec_arr[1] = (host_dec >> 16) & 0xff;
    //     host_dec_arr[2] = (host_dec >> 8) & 0xff;
    //     host_dec_arr[3] = host_dec & 0xff;
    //
    //     std::print(std::cout, "Host: {}.{}.{}.{}\n", host_dec_arr[0], host_dec_arr[1], host_dec_arr[2], host_dec_arr[3]);
    //     std::print(std::cout, "Port: {}\n", port_dec);
    //
    // } catch (const std::exception &e) {
    //     std::cerr << "Exception: " << e.what() << "\n";
    // }

    return 0;
}
