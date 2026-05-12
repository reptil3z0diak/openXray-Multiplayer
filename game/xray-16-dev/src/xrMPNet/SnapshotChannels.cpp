#include "SnapshotChannels.h"

namespace xrmp::rep
{
net::NetMessage SnapshotChannels::makeSnapshotMessage(const SnapshotFrame& frame, const SnapshotQuantizationConfig& config)
{
    return net::NetMessage{
        net::MessageType::SnapshotDelta,
        net::Channel::UnreliableSequenced,
        frame.sequence,
        SnapshotSerializer::serialize(frame, config),
    };
}

bool SnapshotChannels::sendSnapshot(net::ServerTransportSession& session, net::ClientId clientId, const SnapshotFrame& frame,
    std::string* error, const SnapshotQuantizationConfig& config)
{
    return session.send(clientId, net::MessageType::SnapshotDelta, net::Channel::UnreliableSequenced,
        SnapshotSerializer::serialize(frame, config), error);
}

bool SnapshotChannels::sendReliableEvent(
    net::ServerTransportSession& session, net::ClientId clientId, const net::Bytes& payload, std::string* error)
{
    return session.send(clientId, net::MessageType::ReliableEvent, net::Channel::ReliableOrdered, payload, error);
}

bool SnapshotChannels::isSnapshotMessage(const net::NetMessage& message)
{
    return message.type == net::MessageType::SnapshotDelta && message.channel == net::Channel::UnreliableSequenced;
}

bool SnapshotChannels::isReliableEventMessage(const net::NetMessage& message)
{
    return message.type == net::MessageType::ReliableEvent && message.channel == net::Channel::ReliableOrdered;
}

SnapshotFrame SnapshotChannels::decodeSnapshotMessage(const net::NetMessage& message, const SnapshotQuantizationConfig& config)
{
    return SnapshotSerializer::deserialize(message.payload, config);
}
} // namespace xrmp::rep
