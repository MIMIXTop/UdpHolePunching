cmake_minimum_required(VERSION 4.2)
project(udp_client)

set(CMAKE_CXX_STANDARD 23)
find_package(Boost COMPONENTS REQUIRED asio)
find_package(msquic CONFIG REQUIRED)

add_executable(udp_client src/main.cpp
        src/MakeStunRequest.hpp
        src/MakeStunRequest.cpp
        src/Client.cpp
        src/Client.hpp
        src/Types/Type.hpp
        src/Stream.cpp
        src/Stream.hpp
        src/Connection.cpp
        src/Connection.hpp
)

target_link_libraries(udp_client PRIVATE
        Boost::asio
        msquic
)