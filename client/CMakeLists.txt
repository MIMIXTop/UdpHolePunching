cmake_minimum_required(VERSION 4.2)
project(udp_client)

set(CMAKE_CXX_STANDARD 23)
find_package(Boost COMPONENTS REQUIRED asio)

add_executable(udp_client src/main.cpp)

target_link_libraries(udp_client PRIVATE
        Boost::asio
)