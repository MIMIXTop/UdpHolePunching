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
#include <fstream>

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

    Client::Client(asio::io_context &io, std::string_view host, std::string_view port, std::string_view userName, int user_port)
        : io_(io),
          resolver_(io),
          serverEndpoint_(*resolver_.resolve(udp::v4(), host, port).begin()),
          socket_(io), userName_(userName),
          workGuard_(asio::make_work_guard(io)){
        try {
            loadToken();
            socket_.open(udp::v4());
            socket_.bind(udp::endpoint(udp::v4(), user_port));

            localPort_ = socket_.local_endpoint().port();

            std::println("Start listener");
            asio::co_spawn(io, listener(), asio::detached);
            std::println("Continue listener");
            reg_ = new MsQuicRegistration("App", QUIC_EXECUTION_PROFILE_LOW_LATENCY, true);

            MsQuicSettings client_settings{};
            client_settings.SetPeerBidiStreamCount(100);
            client_settings.SetDisconnectTimeoutMs(1000);
            MsQuicCredentialConfig client_credential_config{};
            client_credential_config.Type = QUIC_CREDENTIAL_TYPE_NONE;
            client_credential_config.Flags = QUIC_CREDENTIAL_FLAG_CLIENT
                                             | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;


            std::println("Start msquic client config");
            client_config_ = new MsQuicConfiguration(*reg_, MsQuicAlpn("p2p-node"), client_settings,
                                                     client_credential_config);
            std::println("Sertificate file satus: 0x{:x}", client_config_->GetInitStatus());

            MsQuicSettings server_settings{};
            server_settings.SetPeerBidiStreamCount(100);
            server_settings.SetDisconnectTimeoutMs(1000);
            MsQuicCredentialConfig server_credential_config{};

            static QUIC_CERTIFICATE_FILE cert_file;
            cert_file.CertificateFile = "/home/mimixtop/Project/UdpHolePunching/server.cert";
            cert_file.PrivateKeyFile = "/home/mimixtop/Project/UdpHolePunching/server.key";

            server_credential_config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
            server_credential_config.CertificateFile = &cert_file;
            server_credential_config.Flags = QUIC_CREDENTIAL_FLAG_NONE;

            server_config_ = new MsQuicConfiguration(*reg_, MsQuicAlpn("p2p-node"), server_settings,
                                                     server_credential_config);

            std::println("Sertificate file satus: 0x{:x}", server_config_->GetInitStatus());

            menuThread_ = std::jthread([this] {
                menuLoop();
            });


        } catch (const std::exception &err) {
            std::println("{}", err.what());
        }
    }

    Client::~Client() {
        connection_.reset();
        listener_.reset();

        delete client_config_;
        delete server_config_;
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

                auto response = std::make_unique<StunMessage::StunMessageResponse>(parseRawMessage(recv_buffer));
                dispatchResponse(std::move(response), senderEndpoint);
            }
        } catch (const std::exception &e) {
            std::println(std::cerr, "Client listener error: {}", e.what());
        }
    }

    asio::awaitable<void> Client::sendMessage(std::vector<uint8_t> message) {
        std::println("Send message: {}", message);
        co_await socket_.async_send_to(asio::buffer(message), serverEndpoint_, asio::use_awaitable);
    }

    void Client::menuLoop() {
        std::println("============<Menu>================");
        std::println("Команды : ");
        std::println("1. /login ");
        std::println("2. /list ");
        std::println("3. /connect 'имя_клиента'");
        std::println("4. /exit");

        std::string line;

        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            if (appState_ == AppState::Chat) {
                if (line == "/exit") {
                    appState_ = AppState::Menu;
                    connection_.reset();
                    listener_.reset();
                    std::println("Выход из чата ...");
                    asio::co_spawn(io_, listener(), asio::detached);
                } else {
                    std::unique_lock lk{mutex_};
                    if (connection_) connection_->SendMessage(line);
                }
            } else {
                handleCommand(line);
            }
        }
    }

    void Client::dispatchResponse(std::unique_ptr<StunMessage::StunMessageResponse> response,
                                  const udp::endpoint &endpoint) {
        std::visit(
            Util::match{
                [this](Type::Response::BindingResponse item) {
                    std::array<uint8_t, 4> address{};
                    std::memcpy(&address[0], &item.address, address.size());
                    std::println("Address: {}", address);
                    std::println("Port: {}", item.port);
                    token_ = item.jwtToken;
                    saveToken(token_);
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
                [&](Type::Response::ClientPunch item) {
                    if (appState_ == AppState::Chat) return;

                    std::println("ClientPunch");
                    appState_ = AppState::Chat;

                    uint16_t l_port = endpoint.port();
                    uint32_t l_addr = 0;
                    auto bytes = endpoint.address().to_v4().to_bytes();
                    std::memcpy(&l_addr, bytes.data(), sizeof(l_addr));

                    startConnection(l_port, l_addr, StunMessage::Type::ClientPunch);
                    std::println("\nСоединение готово. Введите сообщение и нажмите Enter для отправки:");

                    startPrint_ = true;

                    cv_.notify_one();
                },
                [&](Type::Response::ServerPunch item) {
                    if (appState_ == AppState::Chat) return;

                    std::println("ServerPunch");
                    appState_ = AppState::Chat;

                    uint16_t l_port = endpoint.port();
                    uint32_t l_addr = 0;
                    auto bytes = endpoint.address().to_v4().to_bytes();
                    std::memcpy(&l_addr, bytes.data(), sizeof(l_addr));

                    startConnection(l_port, l_addr, StunMessage::Type::ServerPunch);
                    std::println("\nСоединение готово. Введите сообщение и нажмите Enter для отправки:");

                    startPrint_ = true;

                    cv_.notify_one();
                },
                [&](const Type::Response::IncomingConnectionRequest& item) {
                    std::println("\n==========================================");
                    std::println("ВХОДЯЩИЙ ЗВОНОК!");
                    std::println("Пользователь [{}] хочет начать чат.", item.clientName);
                    std::println("Введите /accept чтобы принять или /reject чтобы отклонить.");
                    std::println("==========================================\n");

                    pendingIncomingUser_ = item.clientName;
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
            std::println("Address: {}", address);
            udp::endpoint friend_endpoint{asio::ip::make_address(address), connectedPort};
            auto punchMessage = makePunchRequest(role, localPort_, 0);

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

    void Client::saveToken(const std::string_view token) {
        std::ofstream file("/home/mimixtop/Project/UdpHolePunching/client/token.txt");
        if (file.is_open()){file << token;}
        file.close();
    }

    void Client::loadToken() {
        std::ifstream file("/home/mimixtop/Project/UdpHolePunching/client/token.txt");
        if (file.is_open()){file >> token_;}
        file.close();
    }

    void Client::MakeRequest(Type::Request::Attribute attr) {
        uint16_t requestType = 0;
        uint16_t requestSize = 0;
        uint32_t requestCookie = 0x2112A442;
        std::array<uint8_t, 12> txId = make_transaction_identifier();
        std::vector<uint8_t> payload{};

        std::visit(
            Util::match{
                [&](Type::Request::BindingAttribute item) {
                    requestType = static_cast<uint16_t>(StunMessage::Type::BindingRequest);
                    requestSize = item.clientName.size();
                    payload.assign(item.clientName.begin(), item.clientName.end());
                },
                [&](const Type::Request::ConnectToClientAttribute& item) {
                    requestType = static_cast<uint16_t>(StunMessage::Type::ConnectToClient);

                    requestSize = item.clientNameToConnect.size() + 2 + item.jwtToken.size();
                    payload.resize(requestSize);// Для ConnectToClient парсим: [Длина токена (2 байта)][Токен][Имя собеседника]
                    uint16_t tokenSize = item.jwtToken.size();
                    tokenSize = std::byteswap(tokenSize);
                    std::memcpy(&payload[0], &tokenSize, sizeof(tokenSize));
                    auto out_it = std::ranges::copy(item.jwtToken, payload.begin() + 2);
                    std::ranges::copy(item.clientNameToConnect, out_it.out);
                },
                [&](Type::Request::GetConnectedList item) {
                    requestType = static_cast<uint16_t>(StunMessage::Type::GetConnectedList);

                    requestSize = item.jwtToken.size();
                    payload.assign(item.jwtToken.begin(), item.jwtToken.end());
                },
                [&](const Type::Request::ConnectConsent& item) {
                    requestType = static_cast<uint16_t>(StunMessage::Type::ConnectConsent);

                    uint16_t tSize = item.jwtToken.size();
                    requestSize = sizeof(uint16_t) + tSize + 1 + item.targetName.size();
                    payload.resize(requestSize);

                    uint16_t netTokenSize = std::byteswap(tSize);
                    std::memcpy(payload.data(), &netTokenSize, sizeof(netTokenSize));

                    auto out_it = std::ranges::copy(item.jwtToken, payload.begin() + 2).out;

                    *out_it = static_cast<uint8_t>(item.isAccepted);
                    ++out_it;

                    std::ranges::copy(item.targetName, out_it);
                }
            },
            attr
        );

        std::vector<uint8_t> request(20 + requestSize);

        requestType = std::byteswap(requestType);
        requestSize = std::byteswap(requestSize);
        requestCookie = std::byteswap(requestCookie);

        std::memcpy(request.data(), &requestType, sizeof(requestType));
        std::memcpy(request.data() + 2, &requestSize, sizeof(requestSize));
        std::memcpy(request.data() + 4, &requestCookie, sizeof(requestCookie));

        std::ranges::copy(txId, request.begin() + 8);

        if (!payload.empty()) {
            std::ranges::copy(payload, request.begin() + 20);
        }

        asio::co_spawn(
            io_,
            sendMessage(std::move(request)),
            asio::detached
        );
    }

    void Client::startConnection(uint16_t connectedPort, u_int32_t connectedTarget, StunMessage::Type role) {
        socket_.close();
        uint16_t localPort = localPort_;

        QuicAddr localAddress{QUIC_ADDRESS_FAMILY_INET, localPort};

        try {
            switch (role) {
                case StunMessage::Type::ClientPunch: {
                    // server fn
                    auto listen = [](MsQuicListener *ls, void *context, QUIC_LISTENER_EVENT *event) -> QUIC_STATUS {
                        if (event->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
                            auto *self = static_cast<Client *>(context);

                            self->connection_ = std::make_unique<P2P::Connection>(event->NEW_CONNECTION.Connection);
                            QUIC_STATUS status = self->connection_->get_connection().SetConfiguration(
                                *self->server_config_);
                            if (QUIC_FAILED(status)) {
                                std::println(
                                    std::cerr,
                                    "[MsQuic Server] Ошибка SetConfiguration для входящего соединения: 0x{:x}", status);
                            }
                        }

                        return QUIC_STATUS_SUCCESS;
                    };

                    listener_ = std::make_unique<MsQuicListener>(*reg_, CleanUpManual, listen, this);
                    QUIC_STATUS status = listener_->Start(MsQuicAlpn("p2p-node"), &localAddress.SockAddr);

                    if (QUIC_FAILED(status)) {
                        std::println(std::cerr, "[MsQuic Server] Не удалось запустить Listener на порту {}: 0x{:x}",
                                     localPort, status);
                    }
                    break;
                }
                case StunMessage::Type::ServerPunch: {
                    // client fn
                    connection_ = std::make_unique<P2P::Connection>(*reg_);
                    QUIC_STATUS status_addr = connection_->get_connection().SetLocalAddr(QuicAddr{localAddress});

                    if (QUIC_FAILED(status_addr)) {
                        std::println(std::cerr, "[MsQuic Client] Ошибка SetLocalAddr для локального порта {}: 0x{:x}",
                                     localPort, status_addr);
                    }

                    auto target = makeStrAddress(connectedTarget);
                    QUIC_STATUS status = connection_->get_connection().Start(
                        *client_config_, target.c_str(), connectedPort);

                    if (QUIC_FAILED(status)) {
                        std::println(std::cerr, "Ошибка запуска MsQuic подключения: 0x{:x}", status);
                    } else {
                        std::println("MsQuic подключение успешно запущено в фоне.");
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Unknown role");
            }
        } catch (const std::exception &e) {
            std::println("Error: {}", e.what());
        }
    }

    void Client::handleCommand(std::string_view line) {
        if (line == "/login") {
            MakeRequest(Type::Request::BindingAttribute{
                .clientName = userName_
            });
        }
        else if (line == "/list") {
            MakeRequest(Type::Request::GetConnectedList{
                .jwtToken = token_
            });
        }
        else if (line.starts_with("/connect ")) {
            std::string connectName(line.substr(9));
            std::println("Отправка запроса пользователю {}...", connectName);
            startP2P_ = false;
            MakeRequest(Type::Request::ConnectToClientAttribute{
                .clientNameToConnect = connectName,
                .jwtToken = token_
            });
        }
        else if (line == "/accept") {
            if (pendingIncomingUser_.empty()) {
                std::println("У вас нет входящих запросов.");
            } else {
                answerConsent(true);
            }
        }
        else if (line == "/reject") {
            if (pendingIncomingUser_.empty()) {
                std::println("У вас нет входящих запросов.");
            } else {
                answerConsent(false);
            }
        }
        else {
            std::println("Неизвестная команда. Введите /login, /list, /connect ИМЯ, /accept или /reject");
        }

    }

    void Client::answerConsent(bool accept) {
        MakeRequest(Type::Request::ConnectConsent{
            .targetName = pendingIncomingUser_,
            .jwtToken = token_,
            .isAccepted = accept
        });

        if (accept) {
            std::println("Вы приняли запрос. Ожидаем установления соединения...");
        } else {
            std::println("Запрос отклонен.");
        }

        pendingIncomingUser_.clear();
    }
}
