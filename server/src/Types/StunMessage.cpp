#include "StunMessage.hpp"

#include <span>
#include <random>

namespace Network {
    std::array<uint8_t, 20> StunMessage::castHeaderToBytes(const Header &header) {
        std::array<uint8_t, 20> bytes{};
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

    std::vector<uint8_t> make_transaction_identifier() {
        std::vector<uint8_t> identifier(12,0);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution distribution(0, 255);

        for (auto&& item : identifier) {
            item = distribution(gen);
        }

        return identifier;
    }
}
