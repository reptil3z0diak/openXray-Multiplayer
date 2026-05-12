#include "GNSTransport.h"
#include "TransportSession.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace xrmp::net;

namespace
{
Checksum makeDemoChecksum(Byte seed)
{
    Checksum checksum{};
    checksum.fill(seed);
    return checksum;
}

void runServer()
{
    GNSTransport transport;
    std::string error;
    if (!transport.listen(5446, &error))
    {
        std::cerr << "server listen failed: " << error << "\n";
        return;
    }

    ServerSessionConfig config;
    config.handshakePolicy.protocolVersion = ProtocolVersion;
    config.handshakePolicy.buildId = 42;
    config.handshakePolicy.assetChecksum = makeDemoChecksum(0xA1);
    config.handshakePolicy.scriptChecksum = makeDemoChecksum(0xB2);
    config.handshakePolicy.acceptedAuthTokens = { "dev-token" };
    ServerTransportSession session(transport, config);

    while (true)
    {
        while (auto event = session.pump(std::chrono::steady_clock::now(), &error))
        {
            if (event->kind == ServerSessionEvent::Kind::ClientAccepted)
            {
                std::cout << "client " << event->clientId << (event->accept.resumedSession ? " resumed" : " connected")
                          << "\n";
                continue;
            }

            if (event->kind == ServerSessionEvent::Kind::ClientRejected)
            {
                std::cout << "client rejected: " << event->reject.message << "\n";
                continue;
            }

            if (event->kind == ServerSessionEvent::Kind::ClientDisconnected)
            {
                std::cout << "client disconnected: " << event->disconnect.diagnostic << "\n";
                continue;
            }

            if (event->kind == ServerSessionEvent::Kind::Message && event->message.type == MessageType::UserReliable)
                session.send(event->clientId, MessageType::UserReliable, Channel::ReliableOrdered, event->message.payload,
                    &error);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void runClient(const char* host)
{
    GNSTransport transport;
    std::string error;

    ClientSessionConfig config;
    config.endpoint = { host, 5446 };
    config.handshake.protocolVersion = ProtocolVersion;
    config.handshake.buildId = 42;
    config.handshake.assetChecksum = makeDemoChecksum(0xA1);
    config.handshake.scriptChecksum = makeDemoChecksum(0xB2);
    config.handshake.authToken = "dev-token";
    config.reconnect.maxAttempts = 3;

    ClientTransportSession session(transport, config);
    if (!session.start(&error))
    {
        std::cerr << "client connect failed: " << error << "\n";
        return;
    }

    bool pingSent = false;

    while (true)
    {
        while (auto event = session.pump(std::chrono::steady_clock::now(), &error))
        {
            if (event->kind == ClientSessionEvent::Kind::Connected)
            {
                std::cout << "connected as client " << event->accept.assignedClientId
                          << (event->accept.resumedSession ? " (resumed)" : "") << "\n";
                if (!pingSent)
                {
                    Bytes payload{ 'p', 'i', 'n', 'g' };
                    session.send(MessageType::UserReliable, Channel::ReliableOrdered, std::move(payload), &error);
                    pingSent = true;
                }
                continue;
            }

            if (event->kind == ClientSessionEvent::Kind::Message)
            {
                std::cout << "echo received: "
                          << std::string(reinterpret_cast<const char*>(event->message.payload.data()),
                                 event->message.payload.size())
                          << "\n";
                return;
            }

            if (event->kind == ClientSessionEvent::Kind::HandshakeRejected)
            {
                std::cerr << "handshake rejected: " << event->reject.message << "\n";
                return;
            }

            if (event->kind == ClientSessionEvent::Kind::Reconnecting)
            {
                std::cerr << "server disconnected, reconnect attempt " << event->reconnectAttempt << " in "
                          << event->retryDelayMs << " ms\n";
                continue;
            }

            if (event->kind == ClientSessionEvent::Kind::Disconnected)
            {
                std::cerr << "server disconnected: " << event->disconnect.diagnostic << "\n";
                return;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string_view(argv[1]) == "server")
    {
        runServer();
        return 0;
    }

    runClient(argc >= 2 ? argv[1] : "127.0.0.1");
    return 0;
}
