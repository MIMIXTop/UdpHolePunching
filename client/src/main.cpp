#include "MakeStunRequest.hpp"

#include "Client.hpp"

#include <iostream>
#include <print>
#include <boost/asio.hpp>

const MsQuicApi* MsQuic = new MsQuicApi();

constexpr std::string_view PeerId = "";
constexpr int Port = 65432;


int main() {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    if (QUIC_FAILED(MsQuic->GetInitStatus())) {
        std::println("FATAL ERROR: Failed to initialize MsQuic!");
        return -1;
    }

    asio::io_context context{8};
    std::string line;
    std::print("PeerName: ");

    std::getline(std::cin, line);
    try {
        Network::Client client(context, "192.168.1.9", "12345", PeerId);
        context.run();
    } catch (std::exception &e) {
        std::cerr << e.what();
    }

    return 0;
}