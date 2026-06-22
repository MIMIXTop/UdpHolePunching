#include "Client.hpp"
#include "MakeStunRequest.hpp"

#include <iostream>
#include <print>

#include "Util/Match.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <exception>

namespace {
    std::string makeStrAddress(uint32_t target) {
        std::array<uint8_t, 4> address_bytes{};
        std::memcpy(&address_bytes[0], &target, sizeof(target));
        return std::format("{}.{}.{}.{}", address_bytes[0], address_bytes[1], address_bytes[2], address_bytes[3]);
    }

    std::array<uint8_t, 26> makePunchRequest(const Network::StunMessage::Type role, const uint16_t port,
                                             const uint32_t address) {
        std::array<uint8_t, 26> request;

        uint16_t type = static_cast<uint16_t>(role);
        uint16_t len = sizeof(port) + sizeof(address);
        uint32_t cookie = 0x2112A442;
        type = std::byteswap(type);
        len = std::byteswap(len);
        cookie = std::byteswap(cookie);


        std::memcpy(&request[0], &type, sizeof(role));
        std::memcpy(&request[2], &len, sizeof(len));
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        auto vec = Network::make_transaction_identifier();
        std::ranges::copy(vec, request.begin() + 8);
        std::array<uint8_t, 6> attr;

        uint16_t temp_port = std::byteswap(port);
        std::memcpy(&attr[0], &temp_port, sizeof(temp_port));
        uint32_t temp_address = std::byteswap(address);
        std::memcpy(&attr[2], &temp_address, sizeof(temp_address));

        std::ranges::copy(attr, request.begin() + 20);
        return request;
    }
}


namespace Network {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    Client::Client(asio::io_context &io, std::string_view host, std::string_view port, std::string_view userName)
        : io_(io),
          resolver_(io),
          serverEndpoint_(*resolver_.resolve(udp::v4(), host, port).begin()),
          socket_(io), userName_(userName),
          workGuard_(asio::make_work_guard(io)) {
        try {
            socket_.open(udp::v4());
            socket_.set_option(asio::socket_base::reuse_address(true));
            socket_.bind(udp::endpoint(udp::v4(), 0));

            std::println("Start listener");
            asio::co_spawn(io, listener(), asio::detached);
            std::println("Continue listener");
            reg_ = new MsQuicRegistration("App", QUIC_EXECUTION_PROFILE_LOW_LATENCY, true);

            MsQuicSettings settings{};
            MsQuicCredentialConfig credential_config{};
            credential_config.Type = QUIC_CREDENTIAL_TYPE_NONE;
            credential_config.Flags = QUIC_CREDENTIAL_FLAG_CLIENT
                                      | QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED
                                      | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;


            std::println("Start msquic config");
            config_ = new MsQuicConfiguration(*reg_, MsQuicAlpn("p2p-node"), settings, credential_config);
            std::println("Init msquic config");


            menuThread_ = std::jthread([this] {
                menuLoop();
            });

            menuThread_.detach();
        } catch (const std::exception &err) {
            std::println("{}", err.what());
        }
    }

    Client::~Client() {
        connection_.reset();
        listener_.reset();

        delete config_;
        delete reg_;
    }

    asio::awaitable<void> Client::listener() {
        try {
            std::println("Start listen");
            for (;;) {
                if (appState_ == AppState::Chat) co_return;
                std::array<uint8_t, 1024> buffer{};
                udp::endpoint senderEndpoint;
                size_t bytes = 0;
                try {
                    bytes = co_await socket_.async_receive_from(
                        asio::buffer(buffer),
                        senderEndpoint,
                        asio::use_awaitable
                    );
                } catch (const boost::system::system_error &e) {
                    if (e.code() == asio::error::operation_aborted) {
                        std::println("ASIO listener gracefully stopped for MsQuic handover.");
                        co_return;
                    }
                    std::println(std::cerr, "Real Network Error: {}", e.what());
                    co_return;
                }


                if (appState_ == AppState::Chat) continue;

                std::vector<uint8_t> recv_buffer(buffer.begin(), buffer.begin() + bytes);

                std::println("Recv bytes: {}", bytes);

                auto response = std::make_unique<StunMessage::StunMessageResponse>(parseRawMessage(recv_buffer));
                std::println("Message type: {}", static_cast<uint16_t>(response->header.message_type));
                dispatchResponse(std::move(response));
                std::println("2Recv bytes: {}", bytes);
            }
        } catch (const std::exception &e) {
            std::println(std::cerr, "Client listener error: {}", e.what());
        }
    }

    void Client::getListConnectedUsers() {
        std::vector<uint8_t> request(20);
        uint16_t message_type = 0x0002;
        uint16_t message_length = 0;
        uint32_t cookie = 0x2112A442;
        std::array<uint8_t, 12> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);

        asio::co_spawn(io_, sendMessage(std::move(request)), asio::detached);
    }

    void Client::ConnectToClient(std::string_view clientName) {
        std::vector<uint8_t> request(20 + sizeof(uint16_t) + clientName.size());
        uint16_t message_type = 0x0003;
        uint16_t message_length = sizeof(uint16_t) + clientName.size();
        uint32_t cookie = 0x2112A442;
        std::array<uint8_t, 12> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);

        uint16_t name_size = std::byteswap(static_cast<uint16_t>(clientName.size()));
        std::memcpy(&request[20], &name_size, sizeof(name_size));
        std::ranges::copy(clientName, request.begin() + 20 + sizeof(name_size));

        asio::co_spawn(io_, sendMessage(std::move(request)), asio::detached);
    }

    asio::awaitable<void> Client::sendMessage(std::vector<uint8_t> message) {
        std::println("Send message: {}", message);
        co_await socket_.async_send_to(asio::buffer(message), serverEndpoint_, asio::use_awaitable);
    }

    void Client::menuLoop() {
        for (;;) {
            if (appState_ == AppState::Chat) {
                std::println("<Chat>");

                std::string line;
                std::cin.clear();
                while (appState_ == AppState::Chat) {
                    if (std::getline(std::cin, line)) {
                        if (line == "EXIT") {
                            appState_ = AppState::Menu;

                            connection_.reset();
                            listener_.reset();

                            asio::co_spawn(io_, listener(), asio::detached);
                            break;
                        }
                        if (connection_) {
                            connection_->SendMessage(line);
                        }
                    }
                }

                continue;
            }
            int number = 0;
            std::println("1. Connect to server");
            std::println("2. Get a list of users connected to the server");
            std::println("3. Connect to client");
            std::print("Input your choice: ");
            std::cin >> number;
            switch (number) {
                case 1: {
                    std::unique_lock lk(mutex_);
                    bindingRequest();
                    cv_.wait(lk, [this] { return startPrint_; });
                    break;
                }
                case 2: {
                    std::unique_lock lk(mutex_);
                    getListConnectedUsers();
                    cv_.wait(lk, [this] { return startPrint_; });
                    break;
                }
                case 3: {
                    std::unique_lock lk(mutex_);

                    std::string connectName;
                    std::print("Input connect name: ");

                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                    std::getline(std::cin, connectName);
                    startP2P_ = false;

                    ConnectToClient(connectName);

                    cv_.wait(lk, [this] { return startPrint_; });
                    break;
                }
                default:
                    std::println("I dont know");
                    break;
            }
            startPrint_ = false;

            if (appState_ == AppState::Menu) {
                std::print("Tap any button ... ");
                std::cin.clear();
                std::cin.get();
                std::cin.get();
                system("clear");
            }
        }
    }

    void Client::dispatchResponse(std::unique_ptr<StunMessage::StunMessageResponse> response) {
        std::visit(
            Util::match{
                [](Type::Response::BindingResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());
                    std::println("Address: {}", address);
                    std::println("Port: {}", item.port);
                },
                [&](Type::Response::ConnectToClientResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());
                    std::println("Address: {}", address);
                    std::println("Port: {}", item.port);
                    std::println("Client name connected: {}", item.clientName);
                    std::println("ConnectToClientResponse");

                    startP2P_ = !startP2P_;
                    asio::co_spawn(
                        io_,
                        initP2PConnection(item.port, item.address, StunMessage::Type::ClientPunch),
                        asio::detached
                    );
                },
                [](Type::Response::GetConnectedListResponse item) {
                    std::ranges::for_each(item.connectedList, [](auto &item) {
                        std::println("Connected client name: {}", item);
                    });
                },
                [](Type::Response::ErrorResponse item) {
                    std::println("Error: {}", item.error);
                },
                [&](Type::Response::ConnectToHostResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());

                    std::println("Host name: {}", item.clientName);
                    std::println("Host port is {}", item.port);
                    std::println("Host address: {}", address);
                    std::println("ConnectToHostResponse");

                    startP2P_ = !startP2P_;
                    asio::co_spawn(
                        io_,
                        initP2PConnection(item.port, item.address, StunMessage::Type::ServerPunch),
                        asio::detached
                    );
                },
                [this](Type::Response::ClientPunch item) {
                    if (appState_ == AppState::Chat) return;

                    std::println("ClientPunch");
                    appState_ = AppState::Chat;
                    socket_.cancel();

                    startConnection(item.port, item.address, StunMessage::Type::ClientPunch);
                    startPrint_ = true;
                    cv_.notify_one();
                },
                [this](Type::Response::ServerPunch item) {
                    if (appState_ == AppState::Chat) return;

                    std::println("ServerPunch");
                    appState_ = AppState::Chat;
                    socket_.cancel();

                    startConnection(item.port, item.address, StunMessage::Type::ServerPunch);
                    std::println("\nСоединение готово. Введите сообщение и нажмите Enter для отправки:");


                    startPrint_ = true;
                    cv_.notify_one();
                },
            },
            response->attribute
        );
        startPrint_ = true;
        cv_.notify_one();
    }

    asio::awaitable<void> Client::initP2PConnection(uint16_t connectedPort, u_int32_t connectedTarget,
                                                    StunMessage::Type role) {
        try {
            auto address = makeStrAddress(connectedTarget);
            udp::endpoint friend_endpoint{asio::ip::make_address(address), connectedPort};
            auto punchMessage = makePunchRequest(role, socket_.local_endpoint().port(),
                                                 socket_.local_endpoint().address().to_v4().to_uint());

            std::println("Start connect to {}:{}", address, connectedPort);
            asio::steady_timer timer{io_};

            for (int i = 0; i < 10; ++i) {
                if (appState_ == AppState::Chat) break;

                co_await socket_.async_send_to(asio::buffer(punchMessage), friend_endpoint, asio::use_awaitable);

                timer.expires_after(std::chrono::milliseconds(50));
                co_await timer.async_wait(asio::use_awaitable);
            }
        } catch (const std::exception &e) {
            std::println("P2P Connection sender stopped: {}", e.what());
        }
    }

    void Client::startConnection(uint16_t connectedPort, u_int32_t connectedTarget, StunMessage::Type role) {
        uint16_t localPort = socket_.local_endpoint().port();

        QuicAddr localAddress{QUIC_ADDRESS_FAMILY_INET, localPort};


        switch (role) {
            case StunMessage::Type::ClientPunch: {
                // server fn
                auto listen = [](MsQuicListener *ls, void *context, QUIC_LISTENER_EVENT *event) -> QUIC_STATUS {
                    if (event->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
                        auto *self = static_cast<Client *>(context);

                        self->connection_ = std::make_unique<P2P::Connection>(event->NEW_CONNECTION.Connection);
                    }

                    return QUIC_STATUS_SUCCESS;
                };

                listener_ = std::make_unique<MsQuicListener>(reg_, CleanUpAutoDelete, listen, this);
                listener_->Start(MsQuicAlpn("p2p-node"), &localAddress.SockAddr);
                break;
            }
            case StunMessage::Type::ServerPunch: {
                // client fn
                connection_ = std::make_unique<P2P::Connection>(*reg_);
                connection_->get_connection().SetLocalAddr(QuicAddr{localAddress});
                connection_->get_connection().Start(*config_, makeStrAddress(connectedTarget).c_str(), connectedPort);
                break;
            }
            default:
                throw std::runtime_error("Unknown role");
        }
    }

    void Client::bindingRequest() {
        std::vector<uint8_t> request(20 + userName_.size());
        uint16_t message_type = 0x0001;
        uint16_t message_length = userName_.size();
        uint32_t cookie = 0x2112A442;
        std::array<uint8_t, 12> tx_id = make_transaction_identifier();
        message_type = std::byteswap(message_type);
        std::memcpy(&request[0], &message_type, sizeof(message_type));
        message_length = std::byteswap(message_length);
        std::memcpy(&request[2], &message_length, sizeof(message_length));
        cookie = std::byteswap(cookie);
        std::memcpy(&request[4], &cookie, sizeof(cookie));
        std::ranges::copy(tx_id, request.begin() + 8);
        std::ranges::copy(userName_, request.begin() + 20);

        asio::co_spawn(io_, sendMessage(std::move(request)), asio::detached);
    }
}
