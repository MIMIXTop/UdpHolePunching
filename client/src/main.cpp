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

    try {
        Network::Client client(context, "192.168.1.9", "12345", "UserA");
        context.run();
    } catch (std::exception &e) {
        std::cerr << e.what();
    }

    return 0;
}
