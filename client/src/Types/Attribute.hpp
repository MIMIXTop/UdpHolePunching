#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Network::Type {

    namespace Request {

        struct BindingAttribute {
            std::string clientName;
        };

        struct GetConnectedList {
            std::string jwtToken;
        };

        struct ConnectToClientAttribute {
            std::string clientNameToConnect;
            std::string jwtToken;
        };

        struct ConnectConsent {
            std::string targetName;
            std::string jwtToken;
            bool isAccepted;
        };

        using Attribute = std::variant<BindingAttribute, GetConnectedList, ConnectToClientAttribute, ConnectConsent>;

    }

    namespace Response {
        struct BindingResponse {
            uint16_t port;
            uint32_t address;
            std::string jwtToken;
        };
        struct ConnectToClientResponse {
            std::string clientName;
            uint32_t address;
            uint16_t port;
        };

        struct ConnectToHostResponse {
            std::string clientName;
            uint32_t address;
            uint16_t port;
        };

        struct GetConnectedListResponse {
            std::vector<std::string> connectedList;
        };

        struct ErrorResponse {
            std::string error;
        };

        struct ServerPunch {
            uint32_t address;
            uint16_t port;
        };

        struct ClientPunch {
            uint32_t address;
            uint16_t port;
        };

        struct IncomingConnectionRequest {
            std::string clientName;
        };

        using Attribute = std::variant<BindingResponse, GetConnectedListResponse, ErrorResponse, ConnectToClientResponse, ConnectToHostResponse, ServerPunch, ClientPunch, IncomingConnectionRequest>;
    }
}
