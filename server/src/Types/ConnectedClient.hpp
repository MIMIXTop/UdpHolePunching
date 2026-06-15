#pragma once
#include <memory>
#include <string>
#include <boost/asio/ip/udp.hpp>

namespace Network::Type {
    struct ConnectedClient {
        std::string name;
        std::shared_ptr<boost::asio::ip::udp::endpoint> endpoint;
    };
}
