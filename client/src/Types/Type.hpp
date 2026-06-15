#pragma once
#include <string>
#include <boost/asio.hpp>

namespace Network::Type {
    namespace asio = boost::asio;

    struct ConnectionUser {
        std::string username;
        std::vector<asio::ip::address> addresses;
    };
}
