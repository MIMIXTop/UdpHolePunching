#include <iostream>
#include <boost/asio.hpp>

int main() {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    asio::io_context io;
    udp::resolver resolver(io);
    udp::endpoint endpoint = *resolver.resolve(udp::v4(),"127.0.0.1", "12345").begin();

    udp::socket socket(io);
    socket.open(udp::v4());

    for (;;) {
        std::cout << "Input message: ";
        std::string msg;
        std::getline(std::cin, msg);
        socket.send_to(asio::buffer(msg), endpoint);

        std::array<char, 1024> recv_buf;
        udp::endpoint sender_endpoint;
        size_t len = socket.receive_from(asio::buffer(recv_buf), sender_endpoint);

        std::cout << "Server replied: ";
        std::cout.write(recv_buf.data(), len) << "\n";

    }
    return 0;
}
