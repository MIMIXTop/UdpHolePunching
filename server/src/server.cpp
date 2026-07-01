#include "server.hpp"
#include "Types/StunMessage.hpp"
#include "util/Match.hpp"

#include <fast-cpp-csv-parser/csv.h>

#include <iostream>
#include <print>
#include <format>
#include <ranges>
#include <algorithm>
#include <fstream>

namespace {
    constexpr auto CSV_FILE_PATH = "/home/mimixtop/Project/UdpHolePunching/server/data.csv";

    Network::StunMessage::Type bytesToEnumRequest(uint16_t val) {
        switch (static_cast<Network::StunMessage::Type>(val)) {
            case Network::StunMessage::Type::BindingRequest:
            case Network::StunMessage::Type::ConnectToClient:
            case Network::StunMessage::Type::GetConnectedList:
            case Network::StunMessage::Type::ConnectConsent:
                return static_cast<Network::StunMessage::Type>(val);
            default:
                return Network::StunMessage::Type::Error;
        }
    }

    std::string makeStrAddress(uint32_t target) {
        std::array<uint8_t, 4> address_bytes{};
        std::memcpy(&address_bytes[0], &target, sizeof(target));
        return std::format("{}.{}.{}.{}", address_bytes[0], address_bytes[1], address_bytes[2], address_bytes[3]);
    }

    bool sameEndpoint(
        const std::shared_ptr<Network::udp::endpoint>& lhs,
        const std::shared_ptr<Network::udp::endpoint>& rhs) {
        if (!lhs || !rhs) return false;
        return lhs->address() == rhs->address() && lhs->port() == rhs->port();
    }
}

namespace Network {
    Server::Server(asio::io_context &io, short port) : port_(port), socket_(io, udp::endpoint(udp::v4(), port_)), jwtManager_(configuration_["SECRET"]) {

        loadUsers();

        asio::co_spawn(io, listen(), asio::detached);
    }

    asio::awaitable<void> Server::listen() {
        auto executor = co_await asio::this_coro::executor;
        std::array<uint8_t, 1024> buffer{};
        for (;;) {
            udp::endpoint client_endpoint;
            size_t bytes = co_await socket_.async_receive_from(
                asio::buffer(buffer),
                client_endpoint,
                asio::use_awaitable
            );

            std::println("Recv bytes: {}", bytes );

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
                    std::println("BindingRequest Start");
                    response_buffer = handleBindingRequest(message, endpoint);
                    std::println("Binding Request Success");
                    break;
                case StunMessage::Type::GetConnectedList:
                    std::println("GetConnectedList Start");
                    response_buffer = handleGetConnectionList(message, endpoint);
                    std::println("GetConnectedList Success");
                    break;
                case StunMessage::Type::ConnectToClient:
                    std::println("ConnectToClient Start");
                    response_buffer = co_await handleConnectToClient(message, endpoint);
                    std::println("ConnectToClient Success");
                    break;
                case StunMessage::Type::ConnectConsent:
                    std::println("ConnectConsent Start");
                    response_buffer = co_await handleConnectConsent(message, endpoint);
                    std::println("ConnectConsent Success");
                    break;
                default:
                    break;
            }

            if (response_buffer.empty()) co_return;

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

                request.attribute = Type::Request::BindingAttribute{
                    .clientName = std::string(attr.begin(), attr.end())
                };
                break;
            case StunMessage::Type::ConnectToClient: {
                if (attr.size() < sizeof(uint16_t)) {
                    request.attribute = Type::Request::Error{.error = "Empty body"};
                    break;
                }

                uint16_t tokenSize = 0;
                std::memcpy(&tokenSize, &attr[0], sizeof(tokenSize));
                tokenSize = std::byteswap(tokenSize);

                if (attr.size() < sizeof(tokenSize) + tokenSize) {
                    request.attribute = Type::Request::Error {
                        .error = "Invalid format"
                    };
                    break;
                }

                std::string token{attr.begin() + 2, attr.begin() + 2 + tokenSize};
                std::string connectName{attr.begin() + 2 + tokenSize, attr.end()};

                request.attribute = Type::Request::ConnectToClientAttribute{
                    .jwtToken = token,
                    .clientNameToConnect = connectName
                };

                break;
            }
            case StunMessage::Type::GetConnectedList:
                request.attribute = Type::Request::GetConnectedList{
                    .jwtToken = std::string(attr.begin(), attr.end())
                };
                break;
            case StunMessage::Type::ConnectConsent:{
                if (attr.size() < sizeof(uint16_t) + 1) {
                    request.attribute = Type::Request::Error{.error = "Empty body"};
                    break;
                }

                uint16_t tokenSize = 0;
                std::memcpy(&tokenSize, &attr[0], sizeof(tokenSize));
                tokenSize = std::byteswap(tokenSize);

                if (attr.size() < sizeof(tokenSize) + tokenSize + 1) {
                    request.attribute = Type::Request::Error{.error = "Invalid format"};
                    break;
                }

                std::string token{attr.begin() + 2, attr.begin() + 2 + tokenSize};

                bool isAccepted = attr[2 + tokenSize] != 0;

                std::string targetName{attr.begin() + 2 + tokenSize + 1, attr.end()};

                request.attribute = Type::Request::ConnectConsentAttribute{
                    .jwtToken = token,
                    .targetName = targetName,
                    .isAccepted = isAccepted
                };
                break;
            }
            default:
                request.attribute = Type::Request::Error{
                    .error = "Unknow method"
                };
                break;
        }

        return request;
    }

    std::vector<uint8_t> Server::parseStunMessageToRaw(const StunMessage::StunMessageResponse &response) const {
        std::array<uint8_t, 20> header_bytes = StunMessage::castHeaderToBytes(response.header);
        std::vector<uint8_t> body_buffer(response.header.message_length);

        std::visit(
            util::match{
                [&](const Type::Response::BindingResponse &item) {
                    auto temp_port = std::byteswap(item.port);
                    auto temp_address = item.address;

                    body_buffer.resize(6 + item.jwtToken.size());

                    std::memcpy(&body_buffer[0], &temp_port, sizeof(temp_port));
                    std::memcpy(&body_buffer[2], &temp_address, sizeof(temp_address));
                    std::ranges::copy(item.jwtToken, body_buffer.begin() + 6);
                },
                [&](const Type::Response::ConnectToClientResponse &item) {
                    auto temp_port = std::byteswap(item.port);
                    auto temp_address = item.address;
                    body_buffer.resize(6 + item.clientName.size()); // Выделяем память
                    std::memcpy(&body_buffer[0], &temp_port, sizeof(temp_port));
                    std::memcpy(&body_buffer[2], &temp_address, sizeof(temp_address));
                    std::ranges::copy(item.clientName, body_buffer.begin() + 6);
                },
                [&](const Type::Response::GetConnectedListResponse &item) {
                    size_t offset = 0;

                    uint16_t client_count = std::byteswap(static_cast<uint16_t>(connected_clients.size()));
                    std::memcpy(&body_buffer[offset], &client_count, sizeof(client_count));
                    offset += sizeof(client_count);

                    for (auto &&name: item.connectedList) {
                        uint16_t name_size = std::byteswap(static_cast<uint16_t>(name.size()));

                        std::memcpy(&body_buffer[offset], &name_size, sizeof(name_size));
                        offset += sizeof(name_size);

                        std::ranges::copy(name, body_buffer.begin() + offset);
                        offset += name.size();
                    }
                },
                [&](const Type::Response::ErrorResponse &item) {
                    body_buffer.resize(item.error.size());
                    std::ranges::copy(item.error, body_buffer.begin());
                },
                [&](const Type::Response::ConnectConsent& item) {
                },
                [&](const Type::Response::IncomingConnectionRequest& item) {
                    body_buffer.resize(item.clientName.size());
                    std::ranges::copy(item.clientName, body_buffer.begin());
                }
            },
            response.attribute
        );

        std::vector<uint8_t> response_raw(20 + body_buffer.size());

        std::ranges::copy(header_bytes, response_raw.begin());
        std::ranges::copy(body_buffer, response_raw.begin() + 20);

        return response_raw;
    }

    std::vector<uint8_t> Server::handleBindingRequest(
        const StunMessage::StunMessageRequest &message, std::shared_ptr<udp::endpoint> client_endpoint) {
        StunMessage::StunMessageResponse response{};

        auto attr = std::get<Type::Request::BindingAttribute>(message.attribute);

        auto* client = findClient(attr.clientName);
        if (client == nullptr) {
            connected_clients.push_back(Type::ConnectedClient{
                .name = attr.clientName,
                .endpoint = client_endpoint
            });
            client = &connected_clients.back();
        } else {
            client->endpoint = client_endpoint;
        }

        auto tokenObj = jwtManager_.makeToken(attr.clientName);

        if (!tokenObj) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;
            const std::string str{"Internal server error"};
            response.header.message_length = str.size();
            response.attribute = Type::Response::ErrorResponse{
                .error = str
            };

            return parseStunMessageToRaw(response);
        }

        saveUser(*client);

        response.header.message_type = StunMessage::Type::SuccessBinding;
        response.header.message_length = sizeof(uint16_t) + sizeof(uint32_t) + tokenObj->size();
        response.header.cookie = message.header.cookie;
        response.header.tx_id = message.header.tx_id;

        uint32_t client_address = 0;
        auto &&temp = client_endpoint->address().to_v4().to_bytes();
        std::memcpy(&client_address, &temp[0], sizeof(client_address));

        response.attribute = Type::Response::BindingResponse{
            .port = client_endpoint->port(),
            .address = client_address,
            .jwtToken = *tokenObj
        };

        std::println("Jwt token: {}", *tokenObj);

        return parseStunMessageToRaw(response);
    }

    std::vector<uint8_t> Server::handleGetConnectionList(const StunMessage::StunMessageRequest &message,
                                                         std::shared_ptr<udp::endpoint> client_endpoint) {
        StunMessage::StunMessageResponse response{};
        auto attr = std::get<Type::Request::GetConnectedList>(message.attribute);

        auto peerId = jwtManager_.verifyJwt(attr.jwtToken);

        if (!peerId) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;
            const std::string str{"Unauthorized: Invalid or expired token"};
            response.header.message_length = str.size();
            response.attribute = Type::Response::ErrorResponse{
                .error = str
            };

            return parseStunMessageToRaw(response);
        }

        response.header.message_type = StunMessage::Type::SuccessConnectedList;
        response.header.message_length = sizeof(Type::Response::GetConnectedListResponse) * connected_clients.size();

        response.header.cookie = message.header.cookie;
        response.header.tx_id = message.header.tx_id;

        Type::Response::GetConnectedListResponse body;
        body.connectedList.reserve(connected_clients.size());

        uint16_t total_length = sizeof(uint16_t);

        std::ranges::for_each(connected_clients, [&](auto &&item) {
            if (!item.endpoint || item.name == *peerId) return;
            body.connectedList.push_back(item.name);

            total_length += sizeof(uint16_t) + item.name.size();
        });
        response.header.message_length = total_length;
        response.attribute = body;

        return parseStunMessageToRaw(response);
    }

    asio::awaitable<std::vector<uint8_t> > Server::handleConnectToClient(const StunMessage::StunMessageRequest &message,
                                                                         std::shared_ptr<udp::endpoint>
                                                                         client_endpoint) {
        StunMessage::StunMessageResponse response{};

        auto attr = std::get<Type::Request::ConnectToClientAttribute>(message.attribute);

        auto peerIdOpt = jwtManager_.verifyJwt(attr.jwtToken);
        if (!peerIdOpt) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;
            const std::string str{"Unauthorized: Invalid or expired token"};
            response.header.message_length = str.size();
            response.attribute = Type::Response::ErrorResponse{
                .error = str
            };

            co_return parseStunMessageToRaw(response);
        }

        auto it = std::ranges::find(
            connected_clients,
            attr.clientNameToConnect,
            &Type::ConnectedClient::name
        );

        if (it == connected_clients.end() || !it->endpoint) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;

            response.attribute = Type::Response::ErrorResponse{
                .error = "Not found online client"
            };
            response.header.message_length = 23;

            co_return parseStunMessageToRaw(response);
        }

        std::string peerId = *peerIdOpt;
        const auto it_host = std::ranges::find(
            connected_clients,
            peerId,
            &Type::ConnectedClient::name
        );

        StunMessage::StunMessageResponse notify{};
        notify.header.message_type = StunMessage::Type::IncomingConnectionRequest;
        notify.header.message_length = peerIdOpt->size();
        notify.header.cookie = 0x2112A442;
        notify.header.tx_id = StunMessage::make_transaction_identifier();

        notify.attribute = Type::Response::IncomingConnectionRequest{
            .clientName = *peerIdOpt
        };

        co_await socket_.async_send_to(
            asio::buffer(parseStunMessageToRaw(notify)),
            *it->endpoint,
            asio::use_awaitable
        );

        co_return std::vector<uint8_t>{};
    }

    asio::awaitable<std::vector<uint8_t>> Server::handleConnectConsent(const StunMessage::StunMessageRequest &message,
        std::shared_ptr<udp::endpoint> client_endpoint) {

        StunMessage::StunMessageResponse response{};

        auto attr = std::get<Type::Request::ConnectConsentAttribute>(message.attribute);

        auto peerIdOpt = jwtManager_.verifyJwt(attr.jwtToken);
        if (!peerIdOpt) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;
            const std::string str{"Unauthorized: Invalid or expired token"};
            response.header.message_length = str.size();
            response.attribute = Type::Response::ErrorResponse{
                .error = str
            };

            co_return parseStunMessageToRaw(response);
        }

        auto it = std::ranges::find(
            connected_clients,
            attr.targetName,
            &Type::ConnectedClient::name
        );

        if (it == connected_clients.end() || !it->endpoint) {
            response.header.message_type = StunMessage::Type::Error;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;

            response.attribute = Type::Response::ErrorResponse{
                .error = "Not found online client"
            };
            response.header.message_length = 23;

            co_return parseStunMessageToRaw(response);
        }

        if (!attr.isAccepted) {
            response.header.message_type = StunMessage::Type::Error;
            response.attribute = Type::Response::ErrorResponse{.error = "Connection rejected by user"};
            response.header.message_length = 27;
            response.header.cookie = message.header.cookie;
            response.header.tx_id = message.header.tx_id;

            auto raw = parseStunMessageToRaw(response);
            co_await socket_.async_send_to(asio::buffer(raw), *it->endpoint, asio::use_awaitable);

            co_return std::vector<uint8_t>{};
        }

        const std::string& peerId = *peerIdOpt;
        auto &&res_attr = *it;
        const auto it_host = std::ranges::find(
            connected_clients,
            peerId,
            &Type::ConnectedClient::name
        );

        if (it_host != connected_clients.end()) {
            it_host->endpoint = client_endpoint;
            saveUser(*it_host);
            const auto &host = *it_host;
            co_await sendConnectMessage(res_attr, host);
        }

        response.header.message_type = StunMessage::Type::SuccessConnectToClient;
        response.header.message_length = sizeof(uint16_t) + sizeof(uint32_t) + attr.targetName.size();
        response.header.cookie = message.header.cookie;
        response.header.tx_id = message.header.tx_id;


        uint32_t client_address = 0;
        auto temp = res_attr.endpoint->address().to_v4().to_bytes();
        std::memcpy(&client_address, &temp[0], sizeof(client_address));

        response.attribute = Type::Response::ConnectToClientResponse{
            .clientName = attr.targetName,
            .address = client_address,
            .port = res_attr.endpoint->port()
        };

        co_return parseStunMessageToRaw(response);
    }

    asio::awaitable<void> Server::sendConnectMessage(const Type::ConnectedClient &client,
                                                     const Type::ConnectedClient &host) {
        if (!client.endpoint || !host.endpoint) co_return;

        StunMessage::StunMessageResponse response{};

        response.header.message_type = StunMessage::Type::ConnectToHost;
        response.header.message_length = sizeof(uint16_t) + sizeof(uint32_t) + host.name.size();
        response.header.cookie = 0x2112A442;
        response.header.tx_id =  StunMessage::make_transaction_identifier();

        auto addr = std::byteswap(host.endpoint->address().to_v4().to_uint());;

        response.attribute = Type::Response::ConnectToClientResponse{
            .clientName = host.name,
            .address = addr,
            .port = host.endpoint->port()
        };

        auto raw = parseStunMessageToRaw(response);

        co_await socket_.async_send_to(
            asio::buffer(raw),
            *client.endpoint,
            asio::use_awaitable
        );
    }

    void Server::saveUser(const Type::ConnectedClient& user) {
        auto existing = std::ranges::find(connected_clients, user.name, &Type::ConnectedClient::name);
        if (existing == connected_clients.end()) {
            connected_clients.push_back(user);
        } else {
            if (!sameEndpoint(existing->endpoint, user.endpoint)) {
                existing->endpoint = user.endpoint;
            }
        }
        persistUsers();
    }

    void Server::persistUsers() const {
        std::ofstream file(CSV_FILE_PATH, std::ios::trunc);
        if (!file.is_open()) return;

        file << "peerId,address,port\n";
        for (const auto& user : connected_clients) {
            std::string address = "0.0.0.0";
            uint16_t port = 0;
            if (user.endpoint) {
                address = user.endpoint->address().to_string();
                port = user.endpoint->port();
            }
            file << user.name << ','
                 << address << ','
                 << port << '\n';
        }
    }

    void Server::loadUsers() {
        connected_clients.clear();

        try {
            io::CSVReader<3> storage(CSV_FILE_PATH);
            storage.read_header(io::ignore_extra_column, "peerId", "address", "port");

            std::string address, peerId;
            uint16_t port = 0;
            while (storage.read_row(peerId, address, port)) {
                boost::system::error_code ec;
                auto ip = asio::ip::make_address(address, ec);
                std::shared_ptr<udp::endpoint> ep{};
                if (!ec && port != 0) {
                    ep = std::make_shared<udp::endpoint>(ip, port);
                }

                auto* existing = findClient(peerId);
                if (existing == nullptr) {
                    connected_clients.push_back(Type::ConnectedClient{
                        .name = peerId,
                        .endpoint = ep
                    });
                } else if (!sameEndpoint(existing->endpoint, ep)) {
                    existing->endpoint = ep;
                }
            }

            std::println("Загружено {} пользователей из БД.", connected_clients.size());
        } catch (const std::exception& ex) {
            std::println("[БД] Файл не найден или пуст. Начинаем с чистого листа.");
        }
    }
}
