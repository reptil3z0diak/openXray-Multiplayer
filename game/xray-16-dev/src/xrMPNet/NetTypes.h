#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xrmp::net
{
using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using ClientId = std::uint64_t;
using ConnectionId = std::uint64_t;
using Sequence = std::uint32_t;

inline constexpr ClientId InvalidClientId = 0;
inline constexpr ConnectionId InvalidConnectionId = 0;
inline constexpr std::uint16_t ProtocolVersion = 1;
inline constexpr std::uint32_t MaxPayloadBytes = 32 * 1024;
inline constexpr std::size_t ChecksumBytes = 32;
inline constexpr std::size_t AuthTokenMaxBytes = 256;

enum class SessionState : std::uint8_t
{
    Idle = 0,
    ConnectingTransport,
    WaitingHandshake,
    Connected,
    Reconnecting,
    Closed,
    Failed,
};

enum class MessageType : std::uint16_t
{
    Invalid = 0,
    HandshakeRequest = 1,
    HandshakeAccept = 2,
    HandshakeReject = 3,
    DisconnectNotice = 4,
    SnapshotDelta = 10,
    ReliableEvent = 11,
    InputCommand = 12,
    PlayerCorrection = 13,
    HitRequest = 14,
    RpcCall = 15,
    SyncVarUpdate = 16,
    UserReliable = 100,
    UserUnreliable = 101,
};

enum class Channel : std::uint8_t
{
    ReliableOrdered = 0,
    ReliableUnordered = 1,
    UnreliableSequenced = 2,
};

enum class DisconnectReason : std::uint16_t
{
    LocalRequest = 0,
    RemoteClosed = 1,
    Timeout = 2,
    ProtocolMismatch = 3,
    AssetMismatch = 4,
    AuthFailed = 5,
    TransportError = 6,
};

struct TransportEndpoint
{
    std::string host;
    std::uint16_t port = 0;
};

struct TransportMetrics
{
    std::uint32_t rttMs = 0;
    float packetLoss = 0.0f;
    std::uint64_t bytesSent = 0;
    std::uint64_t bytesReceived = 0;
    std::uint32_t packetsSent = 0;
    std::uint32_t packetsReceived = 0;
};

struct NetMessage
{
    MessageType type = MessageType::Invalid;
    Channel channel = Channel::ReliableOrdered;
    Sequence sequence = 0;
    Bytes payload;
};

struct TransportEvent
{
    enum class Kind
    {
        None,
        Connected,
        Disconnected,
        Message,
    };

    Kind kind = Kind::None;
    ConnectionId connection = InvalidConnectionId;
    DisconnectReason reason = DisconnectReason::RemoteClosed;
    std::string diagnostic;
    NetMessage message;
};
} // namespace xrmp::net
