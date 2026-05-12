#pragma once

#include "SnapshotSerializer.h"
#include "TransportSession.h"

namespace xrmp::rep
{
class SnapshotChannels
{
public:
    static net::NetMessage makeSnapshotMessage(const SnapshotFrame& frame,
        const SnapshotQuantizationConfig& config = {});

    static bool sendSnapshot(net::ServerTransportSession& session, net::ClientId clientId, const SnapshotFrame& frame,
        std::string* error, const SnapshotQuantizationConfig& config = {});

    static bool sendReliableEvent(net::ServerTransportSession& session, net::ClientId clientId, const net::Bytes& payload,
        std::string* error);

    static bool isSnapshotMessage(const net::NetMessage& message);
    static bool isReliableEventMessage(const net::NetMessage& message);

    static SnapshotFrame decodeSnapshotMessage(const net::NetMessage& message,
        const SnapshotQuantizationConfig& config = {});
};
} // namespace xrmp::rep
