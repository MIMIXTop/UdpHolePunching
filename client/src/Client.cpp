//
// Created by mimixtop on 14.06.2026.
//

#include "Client.hpp"
#include "MakeStunRequest.hpp"

#include <iostream>
#include <print>

#include "../../server/src/util/Match.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>


namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    Client::Client(asio::io_context &io, std::string_view host, std::string_view port, std::string_view userName)
        : io_(io),
          resolver_(io),
          serverEndpoint_(*resolver_.resolve(udp::v4(), host, port).begin()),
          socket_(io), userName_(userName) {
        socket_.open(udp::v4());
        socket_.bind(udp::endpoint(udp::v4(), 0));

        asio::co_spawn(io, listener(), asio::detached);

        menuThread_ = std::jthread([this] {
            menuLoop();
        });
    }

    asio::awaitable<void> Client::listener() {
        try {
            std::vector<uint8_t> buffer(1024);
            for (;;) {
                udp::endpoint senderEndpoint;
                const size_t bytes = co_await socket_.async_receive_from(
                    asio::buffer(buffer),
                    senderEndpoint,
                    asio::use_awaitable
                );
                std::vector<uint8_t> recv_buffer(buffer.begin(), buffer.begin() + bytes);
                auto response = std::make_unique<StunMessage::StunMessageResponse>(parseRawMessage(recv_buffer));

                dispatchResponse(std::move(response));
            }
        } catch (const std::exception& e) {
            std::println(std::cerr, "Client listener error: {}", e.what());
        }
    }

    void Client::getListConnectedUsers() {
        std::vector<uint8_t> request(20);
        uint16_t message_type = 0x0002;
        uint16_t message_length = 0;
        uint32_t cookie = 0x2112A442;
        std::array<uint8_t, 12> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);

        asio::co_spawn(io_, sendMessage(std::move(request)), asio::detached);
    }

    void Client::ConnectToClient(std::string_view clientName) {
        std::vector<uint8_t> request(20 + sizeof(uint16_t) + clientName.size());
        uint16_t message_type = 0x0003;
        uint16_t message_length = sizeof(uint16_t) + clientName.size();
        uint32_t cookie = 0x2112A442;
        std::array<uint8_t, 12> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);

        uint16_t name_size = std::byteswap(static_cast<uint16_t>(clientName.size()));
        std::memcpy(&request[20], &name_size, sizeof(name_size));
        std::ranges::copy(clientName, request.begin() + 20 + sizeof(name_size));

        asio::co_spawn(io_, sendMessage(std::move(request)), asio::detached);
    }

    asio::awaitable<void> Client::sendMessage(std::vector<uint8_t> message) {
        co_await socket_.async_send_to(asio::buffer(message), serverEndpoint_, asio::use_awaitable);
    }

    void Client::menuLoop() {
        for (;;) {
            int number = 0;
            std::println("1. Connect to server");
            std::println("2. Get a list of users connected to the server");
            std::println("3. Connect to client");
            std::print("Input your choice: ");
            std::cin >> number;
            switch (number) {
                case 1: {
                    std::unique_lock lk(mutex_);
                    bindingRequest();
                    cv_.wait(lk, [this]{return startPrint_;});
                    break;
                }
                case 2: {
                    std::unique_lock lk(mutex_);
                    getListConnectedUsers();
                    cv_.wait(lk, [this]{ return startPrint_; });
                    break;
                }
                case 3: {
                    std::unique_lock lk(mutex_);
                    std::string connectName;
                    std::print("Input connect name: ");
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::getline(std::cin, connectName);
                    ConnectToClient(connectName);
                    cv_.wait(lk, [this]{ return startPrint_; });
                    break;
                }
                default:
                    std::println("I dont know");
                    break;
            }
            startPrint_ = false;
            std::print("Tap any button ... ");
            std::cin.clear();
            std::cin.get();
            std::cin.get();
            system("clear");
        }
    }

    void Client::dispatchResponse(std::unique_ptr<StunMessage::StunMessageResponse> response) {
        std::visit(
            util::match{
                [](Type::Response::BindingResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());
                    std::println("Address: {}", address);
                    std::println("Port: {}", item.port);
                },
                [](Type::Response::ConnectToClientResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());
                    std::println("Address: {}", address);
                    std::println("Port: {}", item.port);
                    std::println("Client name connected: {}", item.clientName);
                },
                [](Type::Response::GetConnectedListResponse item) {
                    std::ranges::for_each(item.connectedList, [](auto& item) {
                        std::println("Connected client name: {}", item);
                    });
                },
                [](Type::Response::ErrorResponse item) {
                    std::println("Error: {}", item.error);
                },
                [](Type::Response::ConnectToHostResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());

                    std::println("Host name: {}", item.clientName);
                    std::println("Host port is {}", item.port);
                    std::println("Host address: {}", address);
                }
            },
            response->attribute
            );
        startPrint_ = true;
        cv_.notify_one();
    }

    void Client::bindingRequest() {
        std::vector<uint8_t> request(20 + userName_.size());
        uint16_t message_type = 0x0001;
        uint16_t message_length = userName_.size();
        uint32_t cookie = 0x2112A442;
        std::array<uint8_t, 12> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);
        std::ranges::copy(userName_, request.begin() + 20);

        asio::co_spawn(io_, sendMessage(std::move(request)), asio::detached);
    }
}
