#include "MakeStunRequest.hpp"

#include "Client.hpp"

#include <iostream>
#include <print>
#include <boost/asio.hpp>
#include <generator>

const MsQuicApi* MsQuic = new MsQuicApi();

constexpr std::string_view PeerId = "UserA";
constexpr int Port = 65432;


int main() {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    if (QUIC_FAILED(MsQuic->GetInitStatus())) {
        std::println("FATAL ERROR: Failed to initialize MsQuic!");
        return -1;
    }

    asio::io_context context{8};

    try {
        Network::Client client(context, "192.168.1.9", "12345", PeerId, Port);
        context.run();
    } catch (std::exception &e) {
        std::cerr << e.what();
    }

    return 0;
}