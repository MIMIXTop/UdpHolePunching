#pragma once

#include <string>
#include <string_view>

namespace Type {
    struct Peer {
        std::string peerId;
        std::string address;
        std::string port;
        std::string expires_after;

        Peer(std::string_view peerId, std::string_view address, std::string_view port, std::string_view expires_after) :
        peerId(peerId), address(address), port(port),  expires_after(expires_after) {}
    };
}