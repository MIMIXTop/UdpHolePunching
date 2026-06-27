#pragma once
#include "config.hpp"

#include <fstream>
#include <print>
#include <ranges>
#include <string>
#include <unordered_map>


namespace Util {
    class Config {
    public:
        Config() {
            const std::string configPath = CONFIG_FILE_PATH;

            std::fstream config_file { configPath };
            if (config_file.is_open()) {
                for (auto&& line : std::views::istream<std::string>(config_file)) {
                    auto pos = line.find('=');
                    if (pos != std::string_view::npos) {
                        variables.emplace(line.substr(0, pos), line.substr(pos + 1));
                    }
                }
            } else {
                std::println("[ConfigParser] ВНИМАНИЕ: Не удалось открыть файл конфигурации: {}", configPath);
            }
        }

        ~Config() = default;

        std::string operator[](const std::string &key) const {
            if (auto it = variables.find(key); it != variables.end()) {
                return it->second;
            }

            return "";
        }

    private:
        std::unordered_map<std::string, std::string> variables;
    };
}
