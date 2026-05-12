#pragma once

#include "ITransport.h"

#include <memory>

namespace xrmp::net
{
class GNSTransport final : public ITransport
{
public:
    GNSTransport();
    ~GNSTransport() override;

    GNSTransport(const GNSTransport&) = delete;
    GNSTransport& operator=(const GNSTransport&) = delete;

    bool listen(std::uint16_t port, std::string* error) override;
    ConnectionId connect(const TransportEndpoint& endpoint, std::string* error) override;
    void disconnect(ConnectionId connection, DisconnectReason reason, std::string_view diagnostic) override;
    void shutdown() override;
    bool send(ConnectionId connection, const NetMessage& message, std::string* error) override;
    std::optional<TransportEvent> poll() override;
    TransportMetrics metrics(ConnectionId connection) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace xrmp::net
