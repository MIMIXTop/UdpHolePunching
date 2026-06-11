#include "server.hpp"

#include <iostream>

Network::Server::Server(asio::io_context &io, short port) : socket_(io, udp::endpoint(udp::v4(), port)) {
    handle_receive();
}

void Network::Server::handle_receive() {
    socket_.async_receive_from(
      asio::buffer(buffer_),
      endpoint_,
      [this](boost::system::error_code ec, std::size_t length) {
          if (!ec && length > 0) {
              handle_send(length);
          }
          handle_receive();
      }
    );
}

void Network::Server::handle_send(std::size_t length) {
    std::cout << "Async received: " << std::string(buffer_.data(), length) << "\n";
    std::cout << "Client address: " << endpoint_.address().to_string() << "\n";
    std::cout << "Client port: " << endpoint_.port() << "\n";

    auto response_msg = std::make_shared<std::string>("Async Echo");

    socket_.async_send_to(
        asio::buffer(buffer_, length),
        endpoint_,
        [this, response_msg](boost::system::error_code ec, std::size_t length) {}
    );
}
