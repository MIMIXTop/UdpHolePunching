#pragma once

#include "Types/Attribute.hpp"

#include <cstdint>
#include <array>
#include <span>

namespace Network {
    namespace StunMessage {
        enum class Type : uint16_t {
            BindingRequest = 0x0001,
            SuccessBinding = 0x0101,
            GetConnectedList = 0x0002,
            SuccessConnectedList = 0x0102,
            ConnectToClient = 0x0003,
            SuccessConnectToClient = 0x0103,
            ConnectToHost = 0x0104,
            Error = 0x1111,
        };

        struct Header {
            Type message_type;
            uint16_t message_length;
            uint32_t cookie;
            std::array<uint8_t, 12> tx_id;
        };

        struct StunMessageRequest {
            Header header;
            Network::Type::Request::Attribute attribute;
        };

        struct StunMessageResponse {
            Header header;
            Network::Type::Response::Attribute attribute;
        };
    }

    std::array<uint8_t, 12> make_transaction_identifier();

    StunMessage::StunMessageResponse parseRawMessage(std::span<uint8_t> data);
}
