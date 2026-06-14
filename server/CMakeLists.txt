cmake_minimum_required(VERSION 4.2)
project(udp_server)

set(CMAKE_CXX_STANDARD 23)
find_package(Boost COMPONENTS REQUIRED asio)

add_executable(udp_server src/main.cpp src/server.cpp
        src/StunMessage.cpp
        src/StunMessage.hpp)

target_link_libraries(udp_server PRIVATE
        Boost::asio
)
