#include "server.hpp"
#include "Types/StunMessage.hpp"
#include "util/Match.hpp"

#include <boost/bind.hpp>
#include <iostream>
#include <print>
#include <charconv>
#include <ranges>
#include <algorithm>
#include <utility>

Network::Server::Server(asio::io_context &io, short port) : port_(port), socket_(io, udp::endpoint(udp::v4(), port_)) {
    asio::co_spawn(io, listen(), asio::detached);
}

namespace Network {
    asio::awaitable<void> Server::listen() {
        auto executor = co_await asio::this_coro::executor;
        std::array<uint8_t, 1024> buffer;
        for (;;) {
            udp::endpoint client_endpoint;
            size_t bytes = co_await socket_.async_receive_from(
                asio::buffer(buffer),
                client_endpoint,
                asio::use_awaitable
            );

            std::vector<uint8_t> recv_buffer(buffer.begin(), buffer.begin() + bytes);

            asio::co_spawn(
                executor,
                worker(recv_buffer, std::make_shared<udp::endpoint>(client_endpoint)),
                asio::detached
            );
        }
    }

    asio::awaitable<void> Server::worker(std::span<uint8_t> data, std::shared_ptr<udp::endpoint> endpoint) {
        try {
            auto message = parse_raw_message(data);

            std::vector<uint8_t> response_buffer{};

            switch (message.header.message_type) {
                case StunMessage::Type::BindingRequest:
                    response_buffer = handle_binding_request(message, endpoint);
                    break;
                default:
                    break;
            }

            co_await socket_.async_send_to(
                asio::buffer(response_buffer),
                endpoint_,
                asio::use_awaitable
            );
        } catch (const std::exception &e) {
            std::println(std::cerr, "Server error: {}", e.what());
        }
    }

    StunMessage::StunMessageRequest Server::parse_raw_message(std::span<uint8_t> data) {
        return {};
    }

    std::vector<uint8_t> Server::handle_binding_request(
        StunMessage::StunMessageRequest message, std::shared_ptr<udp::endpoint> client_endpoint) {
        return std::vector<uint8_t>{};
    }
}

void Network::Server::start_receive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), endpoint_,
        boost::bind(&Server::handle_receive, this,
                    asio::placeholders::error,
                    asio::placeholders::bytes_transferred
        )
    );
}

void Network::Server::handle_send(std::shared_ptr<std::vector<uint8_t> >, const boost::system::error_code &ec,
                                  size_t bytes_transferred) {
    if (ec) {
        std::cerr << "[Сеть] Ошибка отправки: " << ec.message() << "\n";
    }

    start_receive();
}

void Network::Server::handle_receive(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    start_receive();

    if (!ec || ec == asio::error::message_size) {
        std::cout << "Async received: " << std::string(buffer_.data(), bytes_transferred) << "\n";
        std::cout << "Client address: " << endpoint_.address().to_string() << "\n";
        std::cout << "Client port: " << endpoint_.port() << "\n";

        const auto buffer = std::make_shared<std::vector<uint8_t> >(buffer_.begin(),
                                                                    buffer_.begin() + bytes_transferred);
        const auto endpoint = std::make_shared<udp::endpoint>(endpoint_);

        auto result = std::make_shared<std::vector<uint8_t> >(handle_request(buffer, endpoint));

        socket_.async_send_to(
            asio::buffer(*result),
            *endpoint,
            boost::bind(
                &Server::handle_send,
                this,
                result,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred
            )
        );
    } else {
        start_receive();
    }
}

// std::vector<uint8_t> Network::Server::handle_request(std::shared_ptr<std::vector<uint8_t>> data, std::shared_ptr<udp::endpoint> ep) {
//     if (data->empty()) {
//         // ! FIXED:
//         return {};
//     }
//
//     if (data->size() < 20) {
//         // !FIXED:
//         return {};
//     }
//
//     auto message = StunMessage::castBytesToStunMessage(*data);
//
//     switch (message.header.message_type) {
//         case StunMessage::Type::BindingRequest:
//             return handle_binding_request(message.header, std::move(ep));
//         default:
//             return {};
//     }
//     return {};
//
// }


std::vector<uint8_t> Network::Server::handle_binding_request(const StunMessage::Header &header,
                                                             std::shared_ptr<udp::endpoint> ep) {
    auto address_vec = ep->address().to_v4().to_bytes();
    uint32_t address;

    std::memcpy(&address, address_vec.data(), 4);

    uint16_t port = ep->port();
    std::array<uint8_t, 20> header_bytes{};

    uint16_t res_type = static_cast<uint16_t>(StunMessage::Type::SuccessBinding);
    res_type = std::byteswap(res_type);
    std::memcpy(&header_bytes[0], &res_type, sizeof(res_type));
    uint16_t message_size = 12;
    message_size = std::byteswap(message_size);
    std::memcpy(&header_bytes[2], &message_size, sizeof(message_size));
    auto cookie = std::byteswap(header.cookie);

    std::memcpy(&header_bytes[4], &cookie, sizeof(cookie));
    auto tx_id = header.tx_id;
    std::ranges::copy(tx_id, header_bytes.begin() + 8);

    std::array<uint8_t, 12> body_bytes{};
    uint16_t body_type = 0x0020;
    body_type = std::byteswap(body_type);
    std::memcpy(&body_bytes[0], &body_type, sizeof(body_type));
    uint16_t body_size = 8;
    body_size = std::byteswap(body_size);
    std::memcpy(&body_bytes[2], &body_size, sizeof(body_size));
    uint8_t ip_family = 0x01;
    ip_family = std::byteswap(ip_family);
    body_bytes[4] = 0x00;
    std::memcpy(&body_bytes[5], &ip_family, sizeof(ip_family));
    port ^= 0x2112;
    port = std::byteswap(port);
    address ^= cookie;
    std::memcpy(&body_bytes[6], &port, sizeof(port));
    std::memcpy(&body_bytes[8], &address, sizeof(address));

    std::vector<uint8_t> response(header_bytes.size() + body_bytes.size(), 0);
    std::ranges::copy(header_bytes, response.begin());
    std::ranges::copy(body_bytes, response.begin() + header_bytes.size());

    return response;
}
