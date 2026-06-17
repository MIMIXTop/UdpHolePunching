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
            std::println("2. Get a list of users connected to the server");
            std::println("3. Connect to client");
            std::print("Input your choice: ");
            std::cin >> number;
            switch (number) {
                case 1:
                    co_await bindingRequest();
                    break;
                case 2:
                    co_await getListConnectedUsers();
                    break;
                case 3:
                    std::println("Hello PC");
                    break;
                default:
                    std::println("I dont know");
                    break;
            }
            std::print("Tap any button ... ");
            std::cin.ignore();
            std::cin.clear();
            std::cin.get();
            system("clear");
        }
    }

    asio::awaitable<void> Client::getListConnectedUsers() {
        std::array<uint8_t, 20> request{};
        uint16_t message_type = 0x0002;
        uint16_t message_length = 0;
        uint32_t cookie = 0x2112A442;
        std::vector<uint8_t> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);

        std::vector<uint8_t> response = co_await sendMessage(request);
        uint16_t response_type = 0x0000;
        std::memcpy(&response_type, &response[0], sizeof(response_type));
        response_type = std::byteswap(response_type);

        if (static_cast<Type::StunRequestType>(response_type) == Type::StunRequestType::SuccessConnectedList) {
            serverConnected_ = true;
            std::println("Success");
        } else {
            serverConnected_ = false;
            std::println("Failed");
            co_return;
        }

        uint16_t response_size = 0;

        std::memcpy(&response_size, &response[2], sizeof(response_size));

        std::vector<uint8_t> attr{response.begin() + 20, response.begin() + 20 + response_size};

        if (attr.empty()) {
            std::cout << "Attribute is empty" << std::endl;
            co_return;
        }

        size_t offset = 0;

        uint16_t count = 0;
        std::memcpy(&count, &attr[offset], sizeof(count));
        count = std::byteswap(count);
        offset += sizeof(count);

        std::vector<std::string> usersName;

        usersName.reserve(count);

        for (uint16_t i = 0; i < count; ++i) {
            if (offset + sizeof(uint16_t) > attr.size()) {
                break;
            }

            uint16_t name_size = 0;
            std::memcpy(&name_size, &attr[offset], sizeof(name_size));
            name_size = std::byteswap(name_size);
            offset += sizeof(name_size);

            if (offset + name_size > attr.size()) {
                break;
            }

            std::string name(reinterpret_cast<const char*>(&attr[offset]), name_size);
            usersName.push_back(std::move(name));
            offset += name_size;
        }

        std::println("Users name is {}", usersName);
    }

    asio::awaitable<std::vector<uint8_t> > Client::sendMessage(std::span<uint8_t> message) {
        co_await socket_.async_send_to(asio::buffer(message), endpoint_, asio::use_awaitable);
        std::vector<uint8_t> response(40);
        co_await socket_.async_receive_from(asio::buffer(response), endpoint_, asio::use_awaitable);
        co_return response;
    }

    asio::awaitable<void> Client::bindingRequest() {
        std::array<uint8_t, 25> request{};
        uint16_t message_type = 0x0001;
        uint16_t message_length = userName_.size();
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

        if (static_cast<Type::StunRequestType>(response_type) == Type::StunRequestType::SuccessBinding) {
            serverConnected_ = true;
            std::println("Success");
        } else {
            serverConnected_ = false;
            std::println("Failed");
            co_return;
        }

        uint16_t response_size = 0;

        std::memcpy(&response_size, &response[2], sizeof(response_size));

        response_size = std::byteswap(response_size);

        std::vector<uint8_t> attr{response.begin() + 20, response.begin() + 20 + response_size };

        uint16_t port = 0;
        std::array<uint8_t, 4> address{};

        std::memcpy(&port, &attr[0], sizeof(port));
        std::ranges::copy(attr.begin() + 2, attr.end(), address.begin());
        port = std::byteswap(port);

        std::println("Address is {}", address);
        std::println("Port is {}", port);
    }
}
