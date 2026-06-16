#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Network {
    namespace Type {
        enum class StunRequestType : uint16_t {
            BindingRequest = 0x0001,
            SuccessBinding = 0x0101,
            GetConnectedList = 0x0002,
            SuccessConnectedList = 0x0102,
            ConnectToClient = 0x0003,
            SuccessConnectToClient = 0x0103,
            Error = 0x1111,
        };

        struct StunMessageHeader {
            uint16_t message_type;
            uint16_t message_length;
            uint32_t cookie;
            uint8_t transaction_id[12];
        };
    }

    std::vector<uint8_t> make_stun_request(Type::StunRequestType requestType);

    std::vector<uint8_t> make_transaction_identifier();
}
