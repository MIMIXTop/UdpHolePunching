#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace Network{
    namespace Type {
         enum class StunRequestType : uint16_t {
             Request = 0x0001,
             Indication = 0x0002 ,
             Success = 0x0101,
             Error = 0x0102,
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