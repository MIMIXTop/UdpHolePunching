#pragma once

#include <rapidcsv.h>
#include <optional>
#include <string_view>

namespace Util {
    class JwtManager {

    public:
        explicit JwtManager(std::string_view secret);
        std::optional<std::string> verifyJwt(std::string_view jwt);
        std::optional<std::string> makeToken(std::string_view peerId);
    private:
        std::string secret_;
    };
} // Util

