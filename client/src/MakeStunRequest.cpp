#include "MakeStunRequest.hpp"

#include <iostream>
#include <ostream>
#include <print>
#include <random>
#include <netinet/in.h>

namespace {

}

std::vector<uint8_t> Network::make_stun_request(Type::StunRequestType requestType) {
    std::vector<uint8_t> request(20, 0);

    std::vector<uint8_t> tx_id = make_transaction_identifier();
    std::vector<uint8_t> message_length(2, 0);
    const auto message_cookie = 0x2112A442u;

    Type::StunMessageHeader r{};

    r.message_type = htons(0x0001);
    r.message_length = htons(0x0000);
    r.cookie = htonl(message_cookie);
    //r.transaction_id = tx_id;


    std::println(std::cout, "");
}

std::vector<uint8_t> Network::make_transaction_identifier() {
    std::vector<uint8_t> identifier(12,0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution distribution(0, 255);

    for (auto&& item : identifier) {
        item = distribution(gen);
    }

    return identifier;
}