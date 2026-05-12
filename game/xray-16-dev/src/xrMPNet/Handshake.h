#pragma once

#include "NetTypes.h"

#include <array>
#include <optional>

namespace xrmp::net
{
using Checksum = std::array<Byte, ChecksumBytes>;

struct HandshakeRequest
{
    std::uint16_t protocolVersion = ProtocolVersion;
    std::uint32_t buildId = 0;
    Checksum assetChecksum{};
    Checksum scriptChecksum{};
    std::string authToken;
};

struct HandshakeAccept
{
    ClientId assignedClientId = InvalidClientId;
    std::uint32_t serverTickRate = 30;
    std::uint32_t snapshotRate = 20;
    std::string sessionNonce;
};

struct HandshakeReject
{
    DisconnectReason reason = DisconnectReason::ProtocolMismatch;
    std::string message;
};

struct HandshakePolicy
{
    std::uint16_t protocolVersion = ProtocolVersion;
    std::uint32_t buildId = 0;
    Checksum assetChecksum{};
    Checksum scriptChecksum{};
    std::string requiredAuthToken;
};

Bytes serializeHandshakeRequest(const HandshakeRequest& request);
HandshakeRequest deserializeHandshakeRequest(const Bytes& payload);

Bytes serializeHandshakeAccept(const HandshakeAccept& accept);
HandshakeAccept deserializeHandshakeAccept(const Bytes& payload);

Bytes serializeHandshakeReject(const HandshakeReject& reject);
HandshakeReject deserializeHandshakeReject(const Bytes& payload);

// Validates the request against the server policy. Precondition: checksums are already computed over canonical asset/script manifests.
std::optional<HandshakeReject> validateHandshake(const HandshakePolicy& policy, const HandshakeRequest& request);

NetMessage makeHandshakeRequestMessage(const HandshakeRequest& request, Sequence sequence);
NetMessage makeHandshakeAcceptMessage(const HandshakeAccept& accept, Sequence sequence);
NetMessage makeHandshakeRejectMessage(const HandshakeReject& reject, Sequence sequence);
} // namespace xrmp::net
