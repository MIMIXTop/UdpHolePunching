cmake_minimum_required(VERSION 4.2)
project(udp_server)

set(CMAKE_CXX_STANDARD 23)
find_package(Boost COMPONENTS REQUIRED asio)
find_package(msquic CONFIG REQUIRED)

add_executable(udp_server src/main.cpp src/server.cpp
        src/Types/StunMessage.cpp
        src/Types/StunMessage.hpp
        src/Types/ConnectedClient.hpp)

target_link_libraries(udp_server PRIVATE
        Boost::asio
        msquic
)
