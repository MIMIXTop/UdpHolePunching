cmake_minimum_required(VERSION 4.2)
project(udp_server)

set(CMAKE_CXX_STANDARD 23)
find_package(Boost COMPONENTS REQUIRED asio)
find_package(cpp-jwt REQUIRED)
find_package(rapidcsv REQUIRED)
find_path(FAST_CPP_CSV_PARSER_INCLUDE_DIRS "fast-cpp-csv-parser/csv.h")


add_executable(udp_server src/main.cpp src/server.cpp
        src/Types/StunMessage.cpp
        src/Types/StunMessage.hpp
        src/Types/ConnectedClient.hpp
        src/util/JwtManager.cpp
        src/util/JwtManager.hpp
        src/util/Config.hpp
)

target_link_libraries(udp_server PRIVATE
        Boost::asio
        cpp-jwt::cpp-jwt
        rapidcsv::rapidcsv
)

target_include_directories(udp_server PRIVATE
        ${FAST_CPP_CSV_PARSER_INCLUDE_DIRS}
        PUBLIC

        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

configure_file(config.hpp.in ${CMAKE_BINARY_DIR}/config.hpp @ONLY)

