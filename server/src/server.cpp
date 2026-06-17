#include "server.hpp"
#include "Types/StunMessage.hpp"
#include "util/Match.hpp"

#include <boost/bind.hpp>
#include <iostream>
#include <print>
#include <ranges>
#include <algorithm>

namespace {
    Network::StunMessage::Type bytesToEnumRequest(uint16_t val) {
        switch (static_cast<Network::StunMessage::Type>(val)) {
            case Network::StunMessage::Type::BindingRequest:
            case Network::StunMessage::Type::ConnectToClient:
            case Network::StunMessage::Type::GetConnectedList:
                return static_cast<Network::StunMessage::Type>(val);
            default:
                return Network::StunMessage::Type::Error;
        }
    }
}

namespace Network {
    Server::Server(asio::io_context &io, short port) : port_(port), socket_(io, udp::endpoint(udp::v4(), port_)) {
        asio::co_spawn(io, listen(), asio::detached);
    }

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
            auto message = parseRawMessage(data);

            std::vector<uint8_t> response_buffer{};

            switch (message.header.message_type) {
                case StunMessage::Type::BindingRequest:
                    response_buffer = handleBindingRequest(message, endpoint);
                    break;
                case StunMessage::Type::GetConnectedList:
                    response_buffer = handleGetConnectionList(message, endpoint);
                default:
                    break;
            }

            co_await socket_.async_send_to(
                asio::buffer(response_buffer),
                *endpoint,
                asio::use_awaitable
            );
        } catch (const std::exception &e) {
            std::println(std::cerr, "Server error: {}", e.what());
        }
    }

    StunMessage::StunMessageRequest Server::parseRawMessage(std::span<uint8_t> data) {
        StunMessage::StunMessageRequest request{};

        uint16_t message_type = 0;
        std::memcpy(&message_type, &data[0], sizeof(message_type));
        message_type = std::byteswap(message_type);
        request.header.message_type = bytesToEnumRequest(message_type);

        uint16_t message_size = 0;
        std::memcpy(&message_size, &data[2], sizeof(message_size));
        message_size = std::byteswap(message_size);
        request.header.message_length = message_size;

        uint32_t cookie = 0;
        std::memcpy(&cookie, &data[4], sizeof(cookie));
        cookie = std::byteswap(cookie);
        request.header.cookie = cookie;

        std::array<uint8_t, 12> tx_id{};
        std::ranges::copy(data.begin() + 8, data.begin() + 20, tx_id.begin());
        request.header.tx_id = tx_id;

        std::vector<uint8_t> attr(message_size);
        std::ranges::copy_n(data.begin() + 20, message_size, attr.begin());

        switch (request.header.message_type) {
            case StunMessage::Type::BindingRequest:
                if (attr.empty()) {
                    request.attribute = Type::Request::Error{.error = "Empty body"};
                    break;
                }

                request.attribute = Type::Request::BindingAttribute {
                    .clientName = std::string(attr.begin(), attr.end())
                };
                break;
            case StunMessage::Type::ConnectToClient:
                if (attr.empty()) {
                    request.attribute = Type::Request::Error{.error = "Empty body"};
                    break;
                }

                request.attribute = Type::Request::ConnectToClientAttribute{
                    .clientName = std::string(attr.begin(), attr.end())
                };
                break;
            case StunMessage::Type::GetConnectedList:
                if (attr.empty()) {
                    request.attribute = Type::Request::Error{.error = "Empty body"};
                    break;
                }

                request.attribute = Type::Request::BindingAttribute {
                    .clientName = std::string(attr.begin(), attr.end())
                };
                break;
            default:
                request.attribute = Type::Request::Error {
                    .error = "Unknow method"
                };
                break;
        }

        return request;
    }

    std::vector<uint8_t> Server::parseStunMessageToRaw(const StunMessage::StunMessageResponse& response) {

        std::array<uint8_t, 20> header_bytes = StunMessage::castHeaderToBytes(response.header);
        std::vector<uint8_t> body_buffer(response.header.message_length);

        std::visit(
            util::match{
                [&](const Type::Response::BindingResponse& item) {
                    auto temp_port = std::byteswap(item.port);
                    auto temp_address = item.address;
                    std::memcpy(&body_buffer[0], &temp_port, sizeof(temp_port));
                    std::memcpy(&body_buffer[2], &temp_address, sizeof(temp_address));
                },
                [&](const Type::Response::ConnectToClientResponse& item) {
                    auto temp_port = std::byteswap(item.port);
                    auto temp_address = item.address;
                    std::ranges::copy(item.clientName, body_buffer.begin());
                    std::memcpy(&body_buffer[item.clientName.size()], &temp_port, sizeof(temp_port));
                    std::memcpy(&body_buffer[item.clientName.size() + 2], &temp_address, sizeof(temp_address));
                },
                [&](const Type::Response::GetConnectedListResponse& item) {
                    size_t offset = 0;

                    uint16_t client_count = std::byteswap(static_cast<uint16_t>(connected_clients.size()));
                    std::memcpy(&body_buffer[offset], &client_count, sizeof(client_count));
                    offset += sizeof(client_count);

                    for (auto&& name : item.connectedList) {
                        uint16_t name_size = std::byteswap(static_cast<uint16_t>(name.size()));

                        std::memcpy(&body_buffer[offset], &name_size, sizeof(name_size));
                        offset += sizeof(name_size);

                        std::ranges::copy(name, body_buffer.begin() + offset);
                        offset += name.size();
                    }
                },
                [&](const Type::Response::ErrorResponse& item) {},
            },
            response.attribute
        );

        std::vector<uint8_t> response_raw(20 + body_buffer.size());

        std::ranges::copy(header_bytes, response_raw.begin());
        std::ranges::copy(body_buffer, response_raw.begin() + 20);

        return response_raw;
    }

    std::vector<uint8_t> Server::handleBindingRequest(
        const StunMessage::StunMessageRequest& message, std::shared_ptr<udp::endpoint> client_endpoint) {

        StunMessage::StunMessageResponse response{};

        auto attr = std::get<Type::Request::BindingAttribute>(message.attribute);

        auto it = std::ranges::find(
            connected_clients,
            attr.clientName,
            &Type::ConnectedClient::name
        );

        if (it == connected_clients.end()) {
            connected_clients.push_back(Type::ConnectedClient{.name = attr.clientName, .endpoint = client_endpoint});
        }

        response.header.message_type = StunMessage::Type::SuccessBinding;
        response.header.message_length = sizeof(uint16_t) + sizeof(uint32_t);
        response.header.cookie = message.header.cookie;
        response.header.tx_id = message.header.tx_id;

        uint32_t client_address = 0;
        auto&& temp = client_endpoint->address().to_v4().to_bytes();
        std::memcpy(&client_address, &temp[0], sizeof(client_address));

        response.attribute = Type::Response::BindingResponse{
            .port = client_endpoint->port(),
            .address = client_address
        };

        return parseStunMessageToRaw(response);
    }

    std::vector<uint8_t> Server::handleGetConnectionList(const StunMessage::StunMessageRequest &message,
        std::shared_ptr<udp::endpoint> client_endpoint) {

        StunMessage::StunMessageResponse response{};

        response.header.message_type = StunMessage::Type::SuccessConnectedList;
        response.header.message_length = sizeof(Type::Response::GetConnectedListResponse) * connected_clients.size();

        response.header.cookie = message.header.cookie;
        response.header.tx_id = message.header.tx_id;

        Type::Response::GetConnectedListResponse body;
        body.connectedList.reserve(connected_clients.size());

        uint16_t total_length = sizeof(uint16_t);

        std::ranges::for_each(connected_clients, [&](auto&& item) {
            body.connectedList.push_back(item.name);

            total_length += sizeof(uint16_t) + item.name.size();
        });
        response.header.message_length = total_length;
        response.attribute = body;

        return parseStunMessageToRaw(response);
    }

    std::vector<uint8_t> Server::handleConnectToClient(const StunMessage::StunMessageRequest &message,
        std::shared_ptr<udp::endpoint> client_endpoint) {
        StunMessage::StunMessageResponse response{};

        auto attr = std::get<Type::Request::ConnectToClientAttribute>(message.attribute);
        auto it = std::ranges::find(
            connected_clients,
            attr.clientName,
            &Type::ConnectedClient::name
        );

        if (it == connected_clients.end()) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;

            response.attribute = Type::Response::ErrorResponse{
                .error = "Not found client"
            };
            response.header.message_length = 16;

            return parseStunMessageToRaw(response);
        }

        response.header.message_type = StunMessage::Type::SuccessConnectToClient;
        response.header.message_length = sizeof(uint16_t) + sizeof(uint32_t) + attr.clientName.size();
        response.header.cookie = message.header.cookie;
        response.header.tx_id = message.header.tx_id;

        auto&& res_attr = *it;

        uint32_t client_address = 0;
        auto temp = res_attr.endpoint->address().to_v4().to_bytes();
        std::memcpy(&client_address, &temp[0], sizeof(client_address));

        response.attribute = Type::Response::ConnectToClientResponse{
            .clientName = attr.clientName,
            .address = client_address,
            .port =res_attr.endpoint->port()
        };

        return parseStunMessageToRaw(response);
    }
}
