#include "Handshake.h"
#include "NetMessageCodec.h"
#include "TransportSession.h"

#include <deque>
#include <iostream>

using namespace xrmp::net;

namespace
{
Checksum checksum(Byte value)
{
    Checksum out{};
    out.fill(value);
    return out;
}

bool expect(bool value, const char* message)
{
    if (!value)
        std::cerr << "FAILED: " << message << "\n";
    return value;
}

class FakeTransport final : public ITransport
{
public:
    bool listen(std::uint16_t, std::string*) override { return true; }

    ConnectionId connect(const TransportEndpoint&, std::string*) override
    {
        ++connectCalls;
        return nextConnectionId++;
    }

    void disconnect(ConnectionId connection, DisconnectReason reason, std::string_view diagnostic) override
    {
        disconnects.push_back(DisconnectNotice{ reason, std::string(diagnostic) });
        incoming.push_back(TransportEvent{ TransportEvent::Kind::Disconnected, connection, reason, std::string(diagnostic), {} });
    }

    void shutdown() override {}

    bool send(ConnectionId connection, const NetMessage& message, std::string*) override
    {
        sent.emplace_back(connection, message);
        return true;
    }

    std::optional<TransportEvent> poll() override
    {
        if (incoming.empty())
            return std::nullopt;

        TransportEvent event = std::move(incoming.front());
        incoming.pop_front();
        return event;
    }

    TransportMetrics metrics(ConnectionId) const override { return {}; }

    std::deque<TransportEvent> incoming;
    std::vector<std::pair<ConnectionId, NetMessage>> sent;
    std::vector<DisconnectNotice> disconnects;
    std::uint32_t connectCalls = 0;
    ConnectionId nextConnectionId = 1;
};
} // namespace

int main()
{
    HandshakeRequest request;
    request.protocolVersion = ProtocolVersion;
    request.buildId = 7;
    request.assetChecksum = checksum(0x11);
    request.scriptChecksum = checksum(0x22);
    request.configChecksum = checksum(0x33);
    request.authToken = "token";
    request.requestedSessionNonce = "resume-me";

    const NetMessage wireMessage = makeHandshakeRequestMessage(request, 99);
    const Bytes wire = serializeMessage(wireMessage);
    const NetMessage decodedMessage = deserializeMessage(wire);
    const HandshakeRequest decodedRequest = deserializeHandshakeRequest(decodedMessage.payload);

    bool ok = true;
    ok &= expect(decodedMessage.type == MessageType::HandshakeRequest, "message type roundtrip");
    ok &= expect(decodedMessage.channel == Channel::ReliableOrdered, "message channel roundtrip");
    ok &= expect(decodedMessage.sequence == 99, "message sequence roundtrip");
    ok &= expect(decodedRequest.protocolVersion == request.protocolVersion, "protocol version roundtrip");
    ok &= expect(decodedRequest.buildId == request.buildId, "build id roundtrip");
    ok &= expect(decodedRequest.assetChecksum == request.assetChecksum, "asset checksum roundtrip");
    ok &= expect(decodedRequest.scriptChecksum == request.scriptChecksum, "script checksum roundtrip");
    ok &= expect(decodedRequest.configChecksum == request.configChecksum, "config checksum roundtrip");
    ok &= expect(decodedRequest.authToken == request.authToken, "auth token roundtrip");
    ok &= expect(decodedRequest.requestedSessionNonce == request.requestedSessionNonce, "resume nonce roundtrip");

    const HandshakePolicy accepted{ ProtocolVersion, 7, checksum(0x11), checksum(0x22), checksum(0x33), { "token" } };
    ok &= expect(!validateHandshake(accepted, decodedRequest).has_value(), "valid handshake accepted");

    const HandshakePolicy rejected{ ProtocolVersion, 7, checksum(0xFF), checksum(0x22), checksum(0x33), { "token" } };
    const auto rejection = validateHandshake(rejected, decodedRequest);
    ok &= expect(rejection.has_value(), "invalid checksum rejected");
    ok &= expect(rejection && rejection->reason == DisconnectReason::AssetMismatch, "reject reason asset mismatch");

    DisconnectNotice disconnectNotice{ DisconnectReason::Timeout, "timed out" };
    const DisconnectNotice decodedDisconnect =
        deserializeDisconnectNotice(serializeDisconnectNotice(disconnectNotice));
    ok &= expect(decodedDisconnect.reason == disconnectNotice.reason, "disconnect reason roundtrip");
    ok &= expect(decodedDisconnect.diagnostic == disconnectNotice.diagnostic, "disconnect diagnostic roundtrip");

    FakeTransport clientTransport;
    ClientSessionConfig clientConfig;
    clientConfig.endpoint = { "127.0.0.1", 5446 };
    clientConfig.handshake = request;
    clientConfig.reconnect.maxAttempts = 2;
    clientConfig.reconnect.initialDelay = std::chrono::milliseconds(50);
    clientConfig.reconnect.maxDelay = std::chrono::milliseconds(100);

    ClientTransportSession clientSession(clientTransport, clientConfig);
    ok &= expect(clientSession.start(nullptr), "client session start");

    const auto now = std::chrono::steady_clock::now();
    clientTransport.incoming.push_back(
        TransportEvent{ TransportEvent::Kind::Connected, 1, DisconnectReason::RemoteClosed, {}, {} });
    clientSession.pump(now, nullptr);
    ok &= expect(clientTransport.sent.size() == 1, "client sends handshake after transport connect");
    ok &= expect(clientTransport.sent.front().second.type == MessageType::HandshakeRequest, "client handshake message type");

    HandshakeAccept acceptMessage;
    acceptMessage.assignedClientId = 9;
    acceptMessage.serverTickRate = 30;
    acceptMessage.snapshotRate = 20;
    acceptMessage.sessionNonce = "session-9";
    clientTransport.incoming.push_back(TransportEvent{ TransportEvent::Kind::Message, 1,
        DisconnectReason::RemoteClosed, {}, makeHandshakeAcceptMessage(acceptMessage, 2) });
    const auto connectedEvent = clientSession.pump(now, nullptr);
    ok &= expect(connectedEvent.has_value(), "client connected event emitted");
    ok &= expect(connectedEvent && connectedEvent->kind == ClientSessionEvent::Kind::Connected,
        "client connected event kind");
    ok &= expect(clientSession.clientId() == 9, "client stores assigned client id");

    clientTransport.incoming.push_back(
        TransportEvent{ TransportEvent::Kind::Disconnected, 1, DisconnectReason::Timeout, "timeout", {} });
    const auto reconnectEvent = clientSession.pump(now, nullptr);
    ok &= expect(reconnectEvent.has_value(), "client reconnect event emitted");
    ok &= expect(reconnectEvent && reconnectEvent->kind == ClientSessionEvent::Kind::Reconnecting,
        "client reconnect event kind");
    clientSession.pump(now + std::chrono::milliseconds(49), nullptr);
    ok &= expect(clientTransport.connectCalls == 1, "client waits before reconnect attempt");
    clientSession.pump(now + std::chrono::milliseconds(50), nullptr);
    ok &= expect(clientTransport.connectCalls == 2, "client reconnect attempt started");

    FakeTransport serverTransport;
    ServerSessionConfig serverConfig;
    serverConfig.handshakePolicy = accepted;
    serverConfig.reconnectWindow = std::chrono::milliseconds(1000);
    ServerTransportSession serverSession(serverTransport, serverConfig);

    serverTransport.incoming.push_back(TransportEvent{ TransportEvent::Kind::Message, 10,
        DisconnectReason::RemoteClosed, {}, makeHandshakeRequestMessage(request, 1) });
    const auto acceptedEvent = serverSession.pump(now, nullptr);
    ok &= expect(acceptedEvent.has_value(), "server accepted event emitted");
    ok &= expect(acceptedEvent && acceptedEvent->kind == ServerSessionEvent::Kind::ClientAccepted,
        "server accepted event kind");
    ok &= expect(acceptedEvent && acceptedEvent->clientId == 1, "server allocates first client id");

    serverTransport.incoming.push_back(
        TransportEvent{ TransportEvent::Kind::Disconnected, 10, DisconnectReason::Timeout, "timeout", {} });
    const auto disconnectedEvent = serverSession.pump(now, nullptr);
    ok &= expect(disconnectedEvent.has_value(), "server disconnected event emitted");
    ok &= expect(disconnectedEvent && disconnectedEvent->kind == ServerSessionEvent::Kind::ClientDisconnected,
        "server disconnected event kind");

    HandshakeRequest resumedRequest = request;
    resumedRequest.requestedSessionNonce = acceptedEvent ? acceptedEvent->accept.sessionNonce : std::string{};
    serverTransport.incoming.push_back(TransportEvent{ TransportEvent::Kind::Message, 11,
        DisconnectReason::RemoteClosed, {}, makeHandshakeRequestMessage(resumedRequest, 2) });
    const auto resumedEvent = serverSession.pump(now + std::chrono::milliseconds(10), nullptr);
    ok &= expect(resumedEvent.has_value(), "server resumed event emitted");
    ok &= expect(resumedEvent && resumedEvent->kind == ServerSessionEvent::Kind::ClientAccepted,
        "server resumed event kind");
    ok &= expect(resumedEvent && resumedEvent->clientId == 1, "server reuses client id on resume");
    ok &= expect(resumedEvent && resumedEvent->accept.resumedSession, "server marks resumed session");

    return ok ? 0 : 1;
}
