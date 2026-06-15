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

        };

        struct ConnectToClientAttribute {
            std::string clientName;
        };

        struct Error {
            std::string error;
        };

        using Attribute = std::variant<BindingAttribute, GetConnectedList, ConnectToClientAttribute, Error>;

    }

    namespace Response {
        struct BindingResponse {};
        struct ConnectToClientResponse {
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

        using Attribute = std::variant<BindingResponse, GetConnectedListResponse, ErrorResponse>;
    }
}

