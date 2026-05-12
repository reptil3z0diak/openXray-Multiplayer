#include "GNSTransport.h"
#include "Handshake.h"
#include "NetMessageCodec.h"

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

    const HandshakePolicy policy{ ProtocolVersion, 42, makeDemoChecksum(0xA1), makeDemoChecksum(0xB2), "dev-token" };
    ClientId nextClientId = 1;

    while (true)
    {
        while (auto event = transport.poll())
        {
            if (event->kind == TransportEvent::Kind::Disconnected)
            {
                std::cout << "client disconnected: " << event->diagnostic << "\n";
                continue;
            }

            if (event->kind != TransportEvent::Kind::Message ||
                event->message.type != MessageType::HandshakeRequest)
            {
                continue;
            }

            HandshakeRequest request = deserializeHandshakeRequest(event->message.payload);
            if (auto reject = validateHandshake(policy, request))
            {
                transport.send(event->connection, makeHandshakeRejectMessage(*reject, event->message.sequence + 1), &error);
                transport.disconnect(event->connection, reject->reason, reject->message);
                continue;
            }

            HandshakeAccept accept;
            accept.assignedClientId = nextClientId++;
            accept.serverTickRate = 30;
            accept.snapshotRate = 20;
            accept.sessionNonce = "demo-session";
            transport.send(event->connection, makeHandshakeAcceptMessage(accept, event->message.sequence + 1), &error);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void runClient(const char* host)
{
    GNSTransport transport;
    std::string error;
    const ConnectionId connection = transport.connect({ host, 5446 }, &error);
    if (connection == InvalidConnectionId)
    {
        std::cerr << "client connect failed: " << error << "\n";
        return;
    }

    HandshakeRequest request;
    request.protocolVersion = ProtocolVersion;
    request.buildId = 42;
    request.assetChecksum = makeDemoChecksum(0xA1);
    request.scriptChecksum = makeDemoChecksum(0xB2);
    request.authToken = "dev-token";
    transport.send(connection, makeHandshakeRequestMessage(request, 1), &error);

    while (true)
    {
        while (auto event = transport.poll())
        {
            if (event->kind == TransportEvent::Kind::Disconnected)
            {
                std::cerr << "server disconnected: " << event->diagnostic << "\n";
                return;
            }

            if (event->kind == TransportEvent::Kind::Message &&
                event->message.type == MessageType::HandshakeAccept)
            {
                const HandshakeAccept accept = deserializeHandshakeAccept(event->message.payload);
                std::cout << "connected as client " << accept.assignedClientId << "\n";
                return;
            }

            if (event->kind == TransportEvent::Kind::Message &&
                event->message.type == MessageType::HandshakeReject)
            {
                const HandshakeReject reject = deserializeHandshakeReject(event->message.payload);
                std::cerr << "handshake rejected: " << reject.message << "\n";
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
