#pragma once
#include <cstdint>
#include <array>
#include <span>
#include <optional>
#include <vector>

namespace Network {
struct StunMessage {
    enum class Type : uint16_t {
        BindingRequest = 0x0001,
        SuccessBinding = 0x0101,
        Error = 0x0102,
    };

    struct Header {
        Type message_type;
        uint16_t message_length;
        uint32_t cookie;
        std::array<uint8_t, 12> tx_id;
    };
    struct Attribute {
        enum class Type : uint16_t {
            MAPPED_ADDRESS = 0x0001,
            RESPONSE_ADDRESS = 0x0002,
            XOR_MAPPED_ADDRESS = 0x0020
        };

        Type attribute_type;
        uint16_t attribute_length;
        std::vector<uint8_t> attribute_value;

    };

    static std::array<uint8_t, 20> castHeaderToBytes(Header header);
    static StunMessage castBytesToStunMessage(std::span<uint8_t> bytes);

    Header header;
    std::optional<Attribute> attribute;
};
}
