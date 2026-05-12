#pragma once

#include "NetTypes.h"

#include <optional>

namespace xrmp::net
{
class ITransport
{
public:
    virtual ~ITransport() = default;

    // Starts a listening endpoint. Precondition: the transport is not already listening.
    virtual bool listen(std::uint16_t port, std::string* error) = 0;

    // Starts an outbound connection. Precondition: endpoint.host is non-empty and endpoint.port != 0.
    virtual ConnectionId connect(const TransportEndpoint& endpoint, std::string* error) = 0;

    // Closes one connection. The reason is sent best-effort when the backend supports it.
    virtual void disconnect(ConnectionId connection, DisconnectReason reason, std::string_view diagnostic) = 0;

    // Closes all sockets and releases backend resources owned by this transport.
    virtual void shutdown() = 0;

    // Sends one logical message on the requested delivery channel.
    virtual bool send(ConnectionId connection, const NetMessage& message, std::string* error) = 0;

    // Polls one transport event. Call repeatedly until std::nullopt is returned.
    virtual std::optional<TransportEvent> poll() = 0;

    // Returns transport metrics for a live connection. Unknown connections return zero metrics.
    virtual TransportMetrics metrics(ConnectionId connection) const = 0;
};
} // namespace xrmp::net
