//
// Created by mimixtop on 12.06.2026.
//

#include "StunMessage.hpp"

#include <span>

namespace Network {
    std::array<uint8_t, 20> StunMessage::castHeaderToBytes(Header header) {
        std::array<uint8_t, 20> bytes;
        auto type_bytes = static_cast<uint16_t>(header.message_type);

        bytes[0] = (type_bytes >> 8) & 0xff;
        bytes[1] = type_bytes & 0xff;

        bytes[2] = (header.message_length >> 8) & 0xff;
        bytes[3] = header.message_length & 0xff;

        bytes[4] = (header.cookie >> 24) & 0xff;
        bytes[5] = (header.cookie >> 16) & 0xff;
        bytes[6] = (header.cookie >> 8) & 0xff;
        bytes[7] = header.cookie & 0xff;

        std::ranges::copy(header.tx_id, bytes.begin() + 8);

        return bytes;
    }

    StunMessage StunMessage::castBytesToStunMessage(std::span<uint8_t> bytes) {
        StunMessage result;
        auto header_bytes = std::span<uint8_t>{bytes.begin(), bytes.begin() + 20};

        result.header.message_type = static_cast<Type>(header_bytes[0] << 8 | header_bytes[1]);
        result.header.message_length = header_bytes[2] << 8 | header_bytes[3];

        result.header.cookie = header_bytes[4] << 24
                               | header_bytes[5] << 16
                               | header_bytes[6] << 8
                               | header_bytes[7];

        std::copy(header_bytes.begin() + 8, header_bytes.end(), result.header.tx_id.begin());
        if (bytes.size() > 20) {
            auto attr_bytes = bytes.subspan(20);
            if (result.header.message_type == StunMessage::Type::BindingRequest) {
                result.attribute = std::nullopt;
            } else {
                result.attribute = std::nullopt;
            }

        } else {
            result.attribute = std::nullopt;
        }

        return result;
    }
}
