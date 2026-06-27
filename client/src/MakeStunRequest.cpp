#include "MakeStunRequest.hpp"
#include <random>
#include <netinet/in.h>
#include <cstring>
#include <print>
#include <stdexcept>

std::array<uint8_t, 12> Network::make_transaction_identifier() {
    std::array<uint8_t, 12> identifier{};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution distribution(0, 255);

    for (auto&& item : identifier) {
        item = distribution(gen);
    }

    return identifier;
}

Network::StunMessage::StunMessageResponse Network::parseRawMessage(std::span<uint8_t> data) {
    StunMessage::StunMessageResponse response{};

    uint16_t message_type = 0;
    uint16_t message_length = 0;
    uint32_t cookie = 0;
    std::array<uint8_t, 12> tx_id{};

    std::memcpy(&message_type, &data[0], sizeof(message_type));
    std::memcpy(&message_length, &data[2], sizeof(message_length));
    std::memcpy(&cookie, &data[4], sizeof(cookie));
    std::memcpy(&tx_id[0], &data[8], sizeof(tx_id));

    message_type = std::byteswap(message_type);
    message_length = std::byteswap(message_length);
    cookie = std::byteswap(cookie);

    response.header.message_type = static_cast<StunMessage::Type>(message_type);
    response.header.message_length = message_length;
    response.header.cookie = cookie;
    response.header.tx_id = tx_id;

    std::vector<uint8_t> attr(data.begin() + 20, data.begin() + 20 + message_length);

    switch (response.header.message_type) {
        case StunMessage::Type::SuccessBinding: {
            if (attr.size() < 6) {
                throw std::runtime_error("Malformed SuccessBinding packet");
            }

            uint16_t port = 0;
            uint32_t address = 0;
            std::memcpy(&port, &attr[0], sizeof(port));
            std::memcpy(&address, &attr[2], sizeof(address));
            port = std::byteswap(port);

            std::string token;
            std::ranges::copy(attr.begin() + 6, attr.end(), std::back_inserter(token));

            std::println("Jwt token: {}", token);

            response.attribute = Type::Response::BindingResponse {
                .port = port,
                .address = address,
                .jwtToken = token
            };
            break;
        }
        case StunMessage::Type::SuccessConnectedList: {
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

                uint16_t nameSize = 0;
                std::memcpy(&nameSize, &attr[offset], sizeof(nameSize));
                nameSize = std::byteswap(nameSize);
                offset += sizeof(nameSize);

                if (offset + nameSize > attr.size()) {
                    break;
                }

                std::string name(reinterpret_cast<const char *>(&attr[offset]), nameSize);
                usersName.push_back(std::move(name));
                offset += nameSize;
            }

            response.attribute = Type::Response::GetConnectedListResponse{
                .connectedList = usersName
            };
            break;
        }
        case StunMessage::Type::SuccessConnectToClient: {
            uint16_t port = 0;
            uint32_t address = 0;
            std::string clientName;
            std::memcpy(&port, &attr[0], sizeof(port));
            std::memcpy(&address, &attr[2], sizeof(address));
            port = std::byteswap(port);
            std::ranges::copy(
                attr.begin() + sizeof(port) + sizeof(address),
                attr.end(),
                std::back_inserter(clientName)
            );

            response.attribute = Type::Response::ConnectToClientResponse {
                .clientName = clientName,
                .address = address,
                .port = port
            };

            break;
        }
        case StunMessage::Type::ConnectToHost: {
            uint16_t port = 0;
            uint32_t address = 0;
            std::string clientName;
            std::memcpy(&port, &attr[0], sizeof(port));
            std::memcpy(&address, &attr[2], sizeof(address));
            port = std::byteswap(port);
            std::ranges::copy(
                attr.begin() + sizeof(port) + sizeof(address),
                attr.end(),
                std::back_inserter(clientName)
            );

            response.attribute = Type::Response::ConnectToHostResponse {
                .clientName = clientName,
                .address = address,
                .port = port
            };

            break;
        }
        case StunMessage::Type::Error: {
            std::string err;
            std::ranges::copy(attr, std::back_inserter(err));

            response.attribute = Type::Response::ErrorResponse {
                .error = err
            };

            break;
        }
        case StunMessage::Type::ServerPunch: {
            uint16_t port = 0;
            uint32_t address = 0;
            std::memcpy(&port, &attr[0], sizeof(port));
            std::memcpy(&address, &attr[2], sizeof(address));
            port = std::byteswap(port);

            response.attribute = Type::Response::ServerPunch {
                .address = address,
                .port = port
            };

            break;
        }
        case StunMessage::Type::ClientPunch: {
            uint16_t port = 0;
            uint32_t address = 0;
            std::memcpy(&port, &attr[0], sizeof(port));
            std::memcpy(&address, &attr[2], sizeof(address));
            port = std::byteswap(port);

            response.attribute = Type::Response::ClientPunch {
                .address = address,
                .port = port
            };
            break;
        }
        case StunMessage::Type::IncomingConnectionRequest: {
            std::string callerName(attr.begin(), attr.end());

            response.attribute = Type::Response::IncomingConnectionRequest {
                .clientName = callerName
            };
            break;
        }
        default:
            throw std::runtime_error("Unknown message type");

    }

    return response;
}
