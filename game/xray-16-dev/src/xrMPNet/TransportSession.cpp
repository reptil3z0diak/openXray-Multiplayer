#include "TransportSession.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace xrmp::net
{
namespace
{
DisconnectNotice makeDisconnectNotice(DisconnectReason reason, std::string diagnostic)
{
    return DisconnectNotice{ reason, std::move(diagnostic) };
}
} // namespace

ClientTransportSession::ClientTransportSession(ITransport& transport, ClientSessionConfig config)
    : transport_(transport), config_(std::move(config))
{
}

bool ClientTransportSession::start(std::string* error)
{
    reconnectAttempt_ = 0;
    pendingDisconnectNotice_.reset();
    accept_ = {};
    return openConnection(error);
}

std::optional<ClientSessionEvent> ClientTransportSession::pump(
    std::chrono::steady_clock::time_point now, std::string* error)
{
    if (state_ == SessionState::Reconnecting && now >= reconnectAt_)
    {
        if (!openConnection(error))
            state_ = SessionState::Failed;
    }

    const std::optional<TransportEvent> transportEvent = transport_.poll();
    if (!transportEvent)
        return std::nullopt;

    if (connection_ != InvalidConnectionId && transportEvent->connection != connection_ &&
        transportEvent->kind != TransportEvent::Kind::Connected)
    {
        return std::nullopt;
    }

    if (transportEvent->kind == TransportEvent::Kind::Connected)
    {
        connection_ = transportEvent->connection;
        state_ = SessionState::WaitingHandshake;
        pendingDisconnectNotice_.reset();

        if (!transport_.send(connection_, makeHandshakeRequestMessage(config_.handshake, nextSequence_++), error))
        {
            state_ = SessionState::Failed;
            return ClientSessionEvent{ ClientSessionEvent::Kind::Disconnected, state_, connection_, {}, {},
                makeDisconnectNotice(DisconnectReason::TransportError, error ? *error : "handshake send failed") };
        }

        return std::nullopt;
    }

    if (transportEvent->kind == TransportEvent::Kind::Message)
    {
        if (transportEvent->message.type == MessageType::HandshakeAccept)
        {
            accept_ = deserializeHandshakeAccept(transportEvent->message.payload);
            config_.handshake.requestedSessionNonce = accept_.sessionNonce;
            reconnectAttempt_ = 0;
            state_ = SessionState::Connected;

            ClientSessionEvent event;
            event.kind = ClientSessionEvent::Kind::Connected;
            event.state = state_;
            event.connection = connection_;
            event.accept = accept_;
            return event;
        }

        if (transportEvent->message.type == MessageType::HandshakeReject)
        {
            state_ = SessionState::Failed;

            ClientSessionEvent event;
            event.kind = ClientSessionEvent::Kind::HandshakeRejected;
            event.state = state_;
            event.connection = connection_;
            event.reject = deserializeHandshakeReject(transportEvent->message.payload);
            return event;
        }

        if (transportEvent->message.type == MessageType::DisconnectNotice)
        {
            pendingDisconnectNotice_ = deserializeDisconnectNotice(transportEvent->message.payload);
            return std::nullopt;
        }

        if (state_ != SessionState::Connected)
            return std::nullopt;

        ClientSessionEvent event;
        event.kind = ClientSessionEvent::Kind::Message;
        event.state = state_;
        event.connection = connection_;
        event.message = transportEvent->message;
        return event;
    }

    if (transportEvent->kind == TransportEvent::Kind::Disconnected)
    {
        const DisconnectNotice notice = pendingDisconnectNotice_.value_or(
            makeDisconnectNotice(transportEvent->reason, transportEvent->diagnostic));
        pendingDisconnectNotice_.reset();

        const ConnectionId disconnectedConnection = connection_;
        connection_ = InvalidConnectionId;

        if (shouldReconnect(notice) && reconnectAttempt_ < config_.reconnect.maxAttempts)
        {
            ++reconnectAttempt_;
            const auto delay = computeReconnectDelay(reconnectAttempt_);
            reconnectAt_ = now + delay;
            state_ = SessionState::Reconnecting;

            ClientSessionEvent event;
            event.kind = ClientSessionEvent::Kind::Reconnecting;
            event.state = state_;
            event.connection = disconnectedConnection;
            event.disconnect = notice;
            event.reconnectAttempt = reconnectAttempt_;
            event.retryDelayMs = static_cast<std::uint32_t>(delay.count());
            return event;
        }

        state_ = (notice.reason == DisconnectReason::ProtocolMismatch || notice.reason == DisconnectReason::AssetMismatch ||
                     notice.reason == DisconnectReason::AuthFailed)
            ? SessionState::Failed
            : SessionState::Closed;

        ClientSessionEvent event;
        event.kind = ClientSessionEvent::Kind::Disconnected;
        event.state = state_;
        event.connection = disconnectedConnection;
        event.disconnect = notice;
        return event;
    }

    return std::nullopt;
}

bool ClientTransportSession::send(MessageType type, Channel channel, Bytes payload, std::string* error)
{
    if (state_ != SessionState::Connected || connection_ == InvalidConnectionId)
    {
        if (error)
            *error = "client session is not connected";
        return false;
    }

    return transport_.send(connection_, NetMessage{ type, channel, nextSequence_++, std::move(payload) }, error);
}

bool ClientTransportSession::openConnection(std::string* error)
{
    connection_ = transport_.connect(config_.endpoint, error);
    if (connection_ == InvalidConnectionId)
        return false;

    state_ = SessionState::ConnectingTransport;
    return true;
}

bool ClientTransportSession::shouldReconnect(const DisconnectNotice& notice) const
{
    return notice.reason == DisconnectReason::Timeout || notice.reason == DisconnectReason::TransportError ||
        notice.reason == DisconnectReason::RemoteClosed;
}

std::chrono::milliseconds ClientTransportSession::computeReconnectDelay(std::uint32_t attempt) const
{
    const double rawDelay = static_cast<double>(config_.reconnect.initialDelay.count()) *
        std::pow(std::max(1.0f, config_.reconnect.backoffMultiplier), static_cast<double>(attempt - 1));
    const auto bounded = std::clamp<std::int64_t>(static_cast<std::int64_t>(rawDelay),
        config_.reconnect.initialDelay.count(), config_.reconnect.maxDelay.count());
    return std::chrono::milliseconds{ bounded };
}

ServerTransportSession::ServerTransportSession(ITransport& transport, ServerSessionConfig config)
    : transport_(transport), config_(std::move(config))
{
}

std::optional<ServerSessionEvent> ServerTransportSession::pump(
    std::chrono::steady_clock::time_point now, std::string* error)
{
    pruneExpiredSessions(now);

    const std::optional<TransportEvent> transportEvent = transport_.poll();
    if (!transportEvent)
        return std::nullopt;

    if (transportEvent->kind == TransportEvent::Kind::Disconnected)
    {
        const auto found = clientIdByConnection_.find(transportEvent->connection);
        if (found == clientIdByConnection_.end())
            return std::nullopt;

        auto session = sessionsByClientId_.find(found->second);
        if (session == sessionsByClientId_.end())
            return std::nullopt;

        session->second.connected = false;
        session->second.connection = InvalidConnectionId;
        session->second.lastSeen = now;
        clientIdByConnection_.erase(found);

        ServerSessionEvent event;
        event.kind = ServerSessionEvent::Kind::ClientDisconnected;
        event.clientId = session->second.clientId;
        event.connection = transportEvent->connection;
        event.disconnect = makeDisconnectNotice(transportEvent->reason, transportEvent->diagnostic);
        return event;
    }

    if (transportEvent->kind != TransportEvent::Kind::Message)
        return std::nullopt;

    if (transportEvent->message.type == MessageType::DisconnectNotice)
        return std::nullopt;

    if (transportEvent->message.type == MessageType::HandshakeRequest)
    {
        const HandshakeRequest request = deserializeHandshakeRequest(transportEvent->message.payload);
        if (auto reject = validateHandshake(config_.handshakePolicy, request))
        {
            transport_.send(
                transportEvent->connection, makeHandshakeRejectMessage(*reject, nextSequence_++), error);
            transport_.disconnect(transportEvent->connection, reject->reason, reject->message);

            ServerSessionEvent event;
            event.kind = ServerSessionEvent::Kind::ClientRejected;
            event.connection = transportEvent->connection;
            event.request = request;
            event.reject = *reject;
            return event;
        }

        SessionRecord* resumed = tryResumeSession(request, now);
        SessionRecord& session = resumed ? *resumed : createSession(request, now);

        if (session.connected && session.connection != InvalidConnectionId && session.connection != transportEvent->connection)
        {
            transport_.disconnect(session.connection, DisconnectReason::LocalRequest, "superseded by resumed session");
            clientIdByConnection_.erase(session.connection);
        }

        session.authToken = request.authToken;
        session.connected = true;
        session.connection = transportEvent->connection;
        session.lastSeen = now;
        clientIdByConnection_[transportEvent->connection] = session.clientId;

        HandshakeAccept accept;
        accept.assignedClientId = session.clientId;
        accept.serverTickRate = config_.serverTickRate;
        accept.snapshotRate = config_.snapshotRate;
        accept.sessionNonce = session.sessionNonce;
        accept.resumedSession = resumed != nullptr;
        transport_.send(transportEvent->connection, makeHandshakeAcceptMessage(accept, nextSequence_++), error);

        ServerSessionEvent event;
        event.kind = ServerSessionEvent::Kind::ClientAccepted;
        event.clientId = session.clientId;
        event.connection = transportEvent->connection;
        event.request = request;
        event.accept = accept;
        return event;
    }

    const auto found = clientIdByConnection_.find(transportEvent->connection);
    if (found == clientIdByConnection_.end())
    {
        transport_.disconnect(transportEvent->connection, DisconnectReason::ProtocolMismatch,
            "user message received before handshake");
        return std::nullopt;
    }

    ServerSessionEvent event;
    event.kind = ServerSessionEvent::Kind::Message;
    event.clientId = found->second;
    event.connection = transportEvent->connection;
    event.message = transportEvent->message;
    return event;
}

bool ServerTransportSession::send(
    ClientId clientId, MessageType type, Channel channel, Bytes payload, std::string* error)
{
    const auto found = sessionsByClientId_.find(clientId);
    if (found == sessionsByClientId_.end() || !found->second.connected ||
        found->second.connection == InvalidConnectionId)
    {
        if (error)
            *error = "server session client is not connected";
        return false;
    }

    return transport_.send(
        found->second.connection, NetMessage{ type, channel, nextSequence_++, std::move(payload) }, error);
}

void ServerTransportSession::disconnect(ClientId clientId, DisconnectReason reason, std::string_view diagnostic)
{
    const auto found = sessionsByClientId_.find(clientId);
    if (found == sessionsByClientId_.end() || found->second.connection == InvalidConnectionId)
        return;

    transport_.disconnect(found->second.connection, reason, diagnostic);
    clientIdByConnection_.erase(found->second.connection);
    found->second.connection = InvalidConnectionId;
    found->second.connected = false;
}

ClientId ServerTransportSession::clientForConnection(ConnectionId connection) const
{
    const auto found = clientIdByConnection_.find(connection);
    return found == clientIdByConnection_.end() ? InvalidClientId : found->second;
}

ConnectionId ServerTransportSession::connectionForClient(ClientId clientId) const
{
    const auto found = sessionsByClientId_.find(clientId);
    return found == sessionsByClientId_.end() ? InvalidConnectionId : found->second.connection;
}

void ServerTransportSession::pruneExpiredSessions(std::chrono::steady_clock::time_point now)
{
    for (auto it = sessionsByClientId_.begin(); it != sessionsByClientId_.end();)
    {
        if (!it->second.connected && now - it->second.lastSeen > config_.reconnectWindow)
        {
            clientIdBySessionNonce_.erase(it->second.sessionNonce);
            it = sessionsByClientId_.erase(it);
            continue;
        }

        ++it;
    }
}

std::string ServerTransportSession::makeSessionNonce()
{
    std::ostringstream stream;
    stream << "xrmp-" << std::hex << nextSessionNonce_++;
    return stream.str();
}

ServerTransportSession::SessionRecord& ServerTransportSession::createSession(
    const HandshakeRequest& request, std::chrono::steady_clock::time_point now)
{
    SessionRecord session;
    session.clientId = nextClientId_++;
    session.authToken = request.authToken;
    session.sessionNonce = makeSessionNonce();
    session.lastSeen = now;
    session.connected = false;

    const ClientId clientId = session.clientId;
    auto inserted = sessionsByClientId_.emplace(clientId, std::move(session));
    clientIdBySessionNonce_[inserted.first->second.sessionNonce] = clientId;
    return inserted.first->second;
}

ServerTransportSession::SessionRecord* ServerTransportSession::tryResumeSession(
    const HandshakeRequest& request, std::chrono::steady_clock::time_point now)
{
    if (request.requestedSessionNonce.empty())
        return nullptr;

    const auto clientIt = clientIdBySessionNonce_.find(request.requestedSessionNonce);
    if (clientIt == clientIdBySessionNonce_.end())
        return nullptr;

    auto sessionIt = sessionsByClientId_.find(clientIt->second);
    if (sessionIt == sessionsByClientId_.end())
        return nullptr;

    SessionRecord& session = sessionIt->second;
    if (session.authToken != request.authToken)
        return nullptr;

    if (!session.connected && now - session.lastSeen > config_.reconnectWindow)
        return nullptr;

    return &session;
}
} // namespace xrmp::net
