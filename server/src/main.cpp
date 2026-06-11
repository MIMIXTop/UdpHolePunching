#include "server.hpp"

#include <iostream>

int main() {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    try {
        asio::io_context io;
        Network::Server server(io, 12345);
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
