#include "JwtManager.hpp"
#include <jwt/jwt.hpp>

#include <fstream>
#include <algorithm>
#include <chrono>
#include <future>
#include  <print>

namespace Util {
    JwtManager::JwtManager(std::string_view secret) : secret_(secret) {
    }

    std::optional<std::string> JwtManager::verifyJwt(std::string_view jwt) {
        using namespace jwt::params;

        std::error_code ec;

        auto decodeToken = jwt::decode(
            jwt::string_view(jwt),
            algorithms({"HS256"}),
            ec,
            secret(secret_),
            verify(true)
        );

        if (ec) {
            if (ec.value() == static_cast<int>(jwt::VerificationErrc::TokenExpired)) {
                std::println("Отклонено: Срок действия токена истек");
            } else {
                std::println("Отклонено: Невалидный токен {}", ec.message());
            }
            return std::nullopt;
        }

        if (!decodeToken.has_claim("peerId")) {
            std::println("Отклонено: отсутствует поле");
            return std::nullopt;
        }

        return decodeToken.payload().get_claim_value<std::string>("peerId");
    }

    std::optional<std::string> JwtManager::makeToken(std::string_view peerId) {
        using namespace jwt::params;
        using namespace std::chrono_literals;

        jwt::jwt_object obj{
            algorithm(
                jwt::algorithm::HS256
            ),
            secret(secret_)
        };

        obj.add_claim("peerId", std::string(peerId));
        obj.add_claim("exp", std::chrono::system_clock::now() + 1h);

        std::error_code ec;
        auto token = obj.signature(ec);

        if (ec) {
            std::println("Ошибка при создании токена: {}", ec.message());
            return std::nullopt;
        }

        return token;
    }
} // Util
