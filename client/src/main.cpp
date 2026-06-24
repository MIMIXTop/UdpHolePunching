#include "MakeStunRequest.hpp"

#include "Client.hpp"

#include <iostream>
#include <print>
#include <boost/asio.hpp>

const MsQuicApi* MsQuic = new MsQuicApi();

int main() {
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    if (QUIC_FAILED(MsQuic->GetInitStatus())) {
        std::println("FATAL ERROR: Failed to initialize MsQuic!");
        return -1;
    }

    asio::io_context context{8};
    std::string line;
    std::print("PeerName: ");

    std::getline(std::cin, line);
    try {
        Network::Client client(context, "127.0.0.1", "12345", line);
        context.run();
    } catch (std::exception &e) {
        std::cerr << e.what();
    }

    return 0;
}

/*
#include <iostream>
#include <print>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>
#include <msquic.hpp>

const char* ALPN = "p2p-node";

// === 1. КОЛЛБЭК ДЛЯ ПОТОКА НА СТОРОНЕ СЕРВЕРА (Прием данных) ===
QUIC_STATUS QUIC_API ServerStreamCallback(MsQuicStream* stream, void* context, QUIC_STREAM_EVENT* event) {
    switch (event->Type) {
        case QUIC_STREAM_EVENT_RECEIVE: {
            // Читаем все входящие буферы
            for (uint32_t i = 0; i < event->RECEIVE.BufferCount; ++i) {
                std::string_view data(
                    reinterpret_cast<const char*>(event->RECEIVE.Buffers[i].Buffer),
                    event->RECEIVE.Buffers[i].Length
                );
                std::println("[Server] Получено сообщение от клиента: '{}'", data);
            }
            // Подтверждаем получение, чтобы освободить буфер в MsQuic
            stream->ReceiveComplete(event->RECEIVE.TotalBufferLength);
            break;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            std::println("[Server] Клиент закрыл поток для отправки.");
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            std::println("[Server] Поток полностью закрыт.");
            break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

// === 2. КОЛЛБЭК ДЛЯ ПОТОКА НА СТОРОНЕ КЛИЕНТА (Подтверждение отправки) ===
QUIC_STATUS QUIC_API ClientStreamCallback(MsQuicStream* stream, void* context, QUIC_STREAM_EVENT* event) {
    switch (event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            std::println("[Client] Буфер отправлен! Освобождаем выделенную память...");

            // Восстанавливаем указатель на наш буфер из контекста
            auto* buffer = static_cast<QUIC_BUFFER*>(event->SEND_COMPLETE.ClientContext);

            // Безопасно удаляем выделенные ресурсы
            delete[] buffer->Buffer; // Удаляем скопированный текст сообщения
            delete buffer;           // Удаляем саму структуру QUIC_BUFFER
            break;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            std::println("[Client] Сервер закрыл поток.");
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            std::println("[Client] Поток полностью закрыт.");
            break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

// Коллбэк для входящих соединений на сервере
QUIC_STATUS QUIC_API ServerConnectionCallback(MsQuicConnection* connection, void* context, QUIC_CONNECTION_EVENT* event) {
    switch (event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            std::println("[Server] Клиент успешно подключился к серверу!");
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
            std::println("[Server] Клиент открыл новый поток! Настраиваем его...");

            // Оборачиваем входящий поток и задаем ему ServerStreamCallback
            static auto server_stream = std::make_unique<MsQuicStream>(
                event->PEER_STREAM_STARTED.Stream,
                CleanUpManual,
                ServerStreamCallback,
                nullptr
            );
            // Запускаем прием данных на этом потоке
            server_stream->Start();
            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            std::println("[Server] Сбой транспорта: 0x{:x}", event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            std::println("[Server] Соединение полностью закрыто.");
            break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

// Коллбэк для клиента
QUIC_STATUS QUIC_API ClientConnectionCallback(MsQuicConnection* connection, void* context, QUIC_CONNECTION_EVENT* event) {
    switch (event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED: {
            std::println("[Client] Успешно подключились к серверу!");

            // Как только подключились — открываем поток для отправки данных
            std::println("[Client] Открываем поток для передачи данных...");
            static auto client_stream = std::make_unique<MsQuicStream>(
                *connection,
                QUIC_STREAM_OPEN_FLAG_NONE,
                CleanUpManual,
                ClientStreamCallback,
                nullptr
            );
            client_stream->Start();

            // Данные для отправки
            std::string message = "Привет, это зашифрованное сообщение по QUIC!";

            // ВАЖНО: Выделяем память на куче и копируем туда строку,
            // чтобы она жила до срабатывания SEND_COMPLETE в фоновом потоке
            auto* buffer = new QUIC_BUFFER();
            buffer->Length = message.size();
            buffer->Buffer = new uint8_t[message.size()];
            std::memcpy(buffer->Buffer, message.data(), message.size());

            std::println("[Client] Отправляем сообщение: '{}'", message);

            // Отправляем пакет. Флаг QUIC_SEND_FLAG_FIN закроет поток на отправку после передачи
            QUIC_STATUS status = client_stream->Send(buffer, 1, QUIC_SEND_FLAG_FIN, buffer);
            if (QUIC_FAILED(status)) {
                std::println("[Client] Ошибка отправки: 0x{:x}", status);
                delete[] buffer->Buffer;
                delete buffer;
            }
            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            std::println("[Client] Сбой транспорта: 0x{:x}", event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            std::println("[Client] Соединение полностью закрыто.");
            break;
        default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

const MsQuicApi* MsQuic = new MsQuicApi();

int main() {
    try {
        std::println("=== Запуск автономного теста MsQuic ===");

        // Инициализация Registration
        auto reg = std::make_unique<MsQuicRegistration>("App", QUIC_EXECUTION_PROFILE_LOW_LATENCY, true);

        // Настройка Клиента
        MsQuicSettings client_settings{};
        MsQuicCredentialConfig client_cred{};
        client_cred.Type = QUIC_CREDENTIAL_TYPE_NONE;
        client_cred.Flags = QUIC_CREDENTIAL_FLAG_CLIENT
                          | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

        auto client_config = std::make_unique<MsQuicConfiguration>(*reg, MsQuicAlpn(ALPN), client_settings, client_cred);
        if (QUIC_FAILED(client_config->GetInitStatus())) {
            std::println("Ошибка инициализации client_config: 0x{:x}", client_config->GetInitStatus());
            return -1;
        }

        // Настройка Сервера (Слушателя)
        MsQuicSettings server_settings{};
        MsQuicCredentialConfig server_cred{};
        static QUIC_CERTIFICATE_FILE cert_file;
        cert_file.CertificateFile = "/home/mimixtop/Project/UdpHolePunching/server.cert";
        cert_file.PrivateKeyFile = "/home/mimixtop/Project/UdpHolePunching/server.key";

        server_cred.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
        server_cred.CertificateFile = &cert_file;
        server_cred.Flags = QUIC_CREDENTIAL_FLAG_NONE;

        auto server_config = std::make_unique<MsQuicConfiguration>(*reg, MsQuicAlpn(ALPN), server_settings, server_cred);
        if (QUIC_FAILED(server_config->GetInitStatus())) {
            std::println("Ошибка инициализации server_config: 0x{:x}", server_config->GetInitStatus());
            return -1;
        }

        // 1. ЗАПУСК СЛУШАТЕЛЯ (СЕРВЕРА) на порту 45678
        uint16_t server_port = 45678;
        QuicAddr server_addr{QUIC_ADDRESS_FAMILY_INET, server_port};

        auto listen_callback = [](MsQuicListener* ls, void* context, QUIC_LISTENER_EVENT* event) -> QUIC_STATUS {
            if (event->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
                std::println("[Server] Получено новое входящее подключение!");
                auto* config = static_cast<MsQuicConfiguration*>(context);

                // Создаем подключение и задаем обработчик
                static auto conn = std::make_unique<MsQuicConnection>(
                    event->NEW_CONNECTION.Connection,
                    CleanUpManual,
                    ServerConnectionCallback,
                    nullptr
                );

                QUIC_STATUS status = conn->SetConfiguration(*config);
                if (QUIC_FAILED(status)) {
                    std::println("[Server] Ошибка SetConfiguration: 0x{:x}", status);
                }
            }
            return QUIC_STATUS_SUCCESS;
        };

        auto listener = std::make_unique<MsQuicListener>(*reg, CleanUpManual, listen_callback, server_config.get());
        QUIC_STATUS listen_status = listener->Start(MsQuicAlpn(ALPN), &server_addr.SockAddr);
        if (QUIC_FAILED(listen_status)) {
            std::println("Не удалось запустить слушатель: 0x{:x}", listen_status);
            return -1;
        }
        std::println("[Server] Слушатель запущен на локальном порту {}", server_port);

        // Даем серверу немного времени на старт
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 2. ЗАПУСК КЛИЕНТА на локальном порту 45679 и подключение к 127.0.0.1:45678
        uint16_t client_port = 45679;
        QuicAddr client_local_addr{QUIC_ADDRESS_FAMILY_INET, client_port};

        auto client_conn = std::make_unique<MsQuicConnection>(*reg, CleanUpManual, ClientConnectionCallback, nullptr);

        // Привязываем клиента к порту 45679
        QUIC_STATUS addr_status = client_conn->SetLocalAddr(client_local_addr);
        if (QUIC_FAILED(addr_status)) {
            std::println("[Client] Ошибка SetLocalAddr: 0x{:x}", addr_status);
            return -1;
        }

        std::println("[Client] Подключаемся к 127.0.0.1:{} с локального порта {}", server_port, client_port);
        QUIC_STATUS conn_status = client_conn->Start(*client_config, "127.0.0.1", server_port);
        if (QUIC_FAILED(conn_status)) {
            std::println("[Client] Ошибка ConnectionStart: 0x{:x}", conn_status);
            return -1;
        }

        // Ожидаем завершения рукопожатия и отправки
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::println("=== Тест завершен ===");
    } catch (const std::exception& e) {
        std::println("Исключение: {}", e.what());
    }
    return 0;
}*/