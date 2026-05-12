#pragma once

#include "Handshake.h"
#include "ITransport.h"

#include <chrono>
#include <optional>
#include <unordered_map>

namespace xrmp::net
{
struct ReconnectPolicy
{
    std::uint32_t maxAttempts = 4;
    std::chrono::milliseconds initialDelay{ 500 };
    std::chrono::milliseconds maxDelay{ 5000 };
    float backoffMultiplier = 2.0f;
};

struct ClientSessionConfig
{
    TransportEndpoint endpoint;
    HandshakeRequest handshake;
    ReconnectPolicy reconnect;
};

struct ClientSessionEvent
{
    enum class Kind
    {
        Connected,
        HandshakeRejected,
        Reconnecting,
        Disconnected,
        Message,
    };

    Kind kind = Kind::Disconnected;
    SessionState state = SessionState::Idle;
    ConnectionId connection = InvalidConnectionId;
    HandshakeAccept accept;
    HandshakeReject reject;
    DisconnectNotice disconnect;
    NetMessage message;
    std::uint32_t reconnectAttempt = 0;
    std::uint32_t retryDelayMs = 0;
};

struct ServerSessionConfig
{
    HandshakePolicy handshakePolicy;
    std::uint32_t serverTickRate = 30;
    std::uint32_t snapshotRate = 20;
    std::chrono::milliseconds reconnectWindow{ 15000 };
};

struct ServerSessionEvent
{
    enum class Kind
    {
        ClientAccepted,
        ClientRejected,
        ClientDisconnected,
        Message,
    };

    Kind kind = Kind::Message;
    ClientId clientId = InvalidClientId;
    ConnectionId connection = InvalidConnectionId;
    HandshakeRequest request;
    HandshakeAccept accept;
    HandshakeReject reject;
    DisconnectNotice disconnect;
    NetMessage message;
};

class ClientTransportSession final
{
public:
    explicit ClientTransportSession(ITransport& transport, ClientSessionConfig config);

    // Starts the first outbound transport connection. Precondition: endpoint host/port are valid.
    bool start(std::string* error);

    // Pumps one transport/session step and may schedule a reconnect when the current link dies.
    std::optional<ClientSessionEvent> pump(std::chrono::steady_clock::time_point now, std::string* error);

    // Sends one user payload once the handshake is established.
    bool send(MessageType type, Channel channel, Bytes payload, std::string* error);

    SessionState state() const { return state_; }
    ConnectionId connection() const { return connection_; }
    ClientId clientId() const { return accept_.assignedClientId; }
    const HandshakeAccept& negotiatedSession() const { return accept_; }

private:
    bool openConnection(std::string* error);
    bool shouldReconnect(const DisconnectNotice& notice) const;
    std::chrono::milliseconds computeReconnectDelay(std::uint32_t attempt) const;

    ITransport& transport_;
    ClientSessionConfig config_;
    SessionState state_ = SessionState::Idle;
    ConnectionId connection_ = InvalidConnectionId;
    Sequence nextSequence_ = 1;
    std::uint32_t reconnectAttempt_ = 0;
    std::chrono::steady_clock::time_point reconnectAt_{};
    HandshakeAccept accept_{};
    std::optional<DisconnectNotice> pendingDisconnectNotice_;
};

class ServerTransportSession final
{
public:
    explicit ServerTransportSession(ITransport& transport, ServerSessionConfig config);

    // Polls the transport and resolves handshake, resume, and user-message dispatch.
    std::optional<ServerSessionEvent> pump(std::chrono::steady_clock::time_point now, std::string* error);

    // Sends one user payload to an already accepted client.
    bool send(ClientId clientId, MessageType type, Channel channel, Bytes payload, std::string* error);

    // Closes a client session. The reconnect window remains available until it expires.
    void disconnect(ClientId clientId, DisconnectReason reason, std::string_view diagnostic);

    ClientId clientForConnection(ConnectionId connection) const;
    ConnectionId connectionForClient(ClientId clientId) const;

private:
    struct SessionRecord
    {
        ClientId clientId = InvalidClientId;
        std::string authToken;
        std::string sessionNonce;
        ConnectionId connection = InvalidConnectionId;
        std::chrono::steady_clock::time_point lastSeen{};
        bool connected = false;
    };

    void pruneExpiredSessions(std::chrono::steady_clock::time_point now);
    std::string makeSessionNonce();
    SessionRecord& createSession(const HandshakeRequest& request, std::chrono::steady_clock::time_point now);
    SessionRecord* tryResumeSession(const HandshakeRequest& request, std::chrono::steady_clock::time_point now);

    ITransport& transport_;
    ServerSessionConfig config_;
    ClientId nextClientId_ = 1;
    Sequence nextSequence_ = 1;
    std::uint64_t nextSessionNonce_ = 1;
    std::unordered_map<ClientId, SessionRecord> sessionsByClientId_;
    std::unordered_map<ConnectionId, ClientId> clientIdByConnection_;
    std::unordered_map<std::string, ClientId> clientIdBySessionNonce_;
};
} // namespace xrmp::net
