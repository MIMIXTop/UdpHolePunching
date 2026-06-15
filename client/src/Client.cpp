//
// Created by mimixtop on 14.06.2026.
//

#include "Client.hpp"
#include "MakeStunRequest.hpp"

#include <iostream>
#include <print>


namespace asio = boost::asio;
using udp = asio::ip::udp;

namespace Network {
    Client::Client(asio::io_context &io, std::string_view host, std::string_view port, std::string_view userName)
        : resolver_(io),
          endpoint_(*resolver_.resolve(udp::v4(), host, port).begin()),
          socket_(io), userName_(userName) {
        socket_.open(udp::v4());
    }

    asio::awaitable<void> Client::listener() {
        for (;;) {
            int number = 0;
            std::println("1. Connect to server");
            std::println("2. I'm a teapod");
            std::println("3. Hello PC");
            std::print("Input your choice: ");
            std::cin >> number;
            switch (number) {
                case 1:
                    co_await bindingRequest();
                    break;
                case 2:
                    std::println("I'm a teapod");
                    break;
                case 3:
                    std::println("Hello PC");
                    break;
                default:
                    std::println("I dont know");
                    break;
            }
            std::print("Tap any button ... ");
            std::cin >> number;
            system("clear");
        }
    }

    asio::awaitable<std::vector<uint8_t> > Client::sendMessage(std::span<uint8_t> message) {
        co_await socket_.async_send_to(asio::buffer(message), endpoint_, asio::use_awaitable);
        udp::endpoint endpoint_receive;
        std::vector<uint8_t> response(32);
        co_await socket_.async_receive_from(asio::buffer(response), endpoint_receive, asio::use_awaitable);
        co_return response;
    }

    asio::awaitable<void> Client::bindingRequest() {
        std::array<uint8_t, 25> request{};
        uint16_t message_type = 0x0001;
        uint16_t message_length = 0x0000;
        uint32_t cookie = 0x2112A442;
        std::vector<uint8_t> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);
        std::ranges::copy(userName_, request.begin() + 20);


        std::vector<uint8_t> response = co_await sendMessage(request);
        uint16_t response_type = 0x0000;
        std::memcpy(&response_type, &response[0], sizeof(response_type));


        if (static_cast<Type::StunRequestType>(response_type) == Type::StunRequestType::Success) {
            serverConnected_ = true;
            std::println("Success");
        } else {
            serverConnected_ = false;
            std::println("Failed");
        }
    }
}
