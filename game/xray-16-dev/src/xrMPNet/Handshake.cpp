#include "Handshake.h"

#include "ByteStream.h"

#include <algorithm>
#include <string_view>
#include <stdexcept>

namespace xrmp::net
{
namespace
{
void writeChecksum(ByteWriter& writer, const Checksum& checksum)
{
    writer.writeBytes(checksum.data(), checksum.size());
}

Checksum readChecksum(ByteReader& reader)
{
    Checksum checksum{};
    const Bytes bytes = reader.readBytes(checksum.size());
    std::copy(bytes.begin(), bytes.end(), checksum.begin());
    return checksum;
}

Bytes serializeRejectPayload(DisconnectReason reason, std::string_view message)
{
    Bytes payload;
    ByteWriter writer(payload);
    writer.writePod(static_cast<std::uint16_t>(reason));
    writer.writeString(message);
    return payload;
}
} // namespace

Bytes serializeHandshakeRequest(const HandshakeRequest& request)
{
    if (request.authToken.size() > AuthTokenMaxBytes)
        throw std::length_error("handshake auth token exceeds AuthTokenMaxBytes");

    Bytes payload;
    ByteWriter writer(payload);
    writer.writePod(request.protocolVersion);
    writer.writePod(request.buildId);
    writeChecksum(writer, request.assetChecksum);
    writeChecksum(writer, request.scriptChecksum);
    writer.writeString(request.authToken);
    writer.writeString(request.requestedSessionNonce);
    return payload;
}

HandshakeRequest deserializeHandshakeRequest(const Bytes& payload)
{
    ByteReader reader(payload);
    HandshakeRequest request;
    request.protocolVersion = reader.readPod<std::uint16_t>();
    request.buildId = reader.readPod<std::uint32_t>();
    request.assetChecksum = readChecksum(reader);
    request.scriptChecksum = readChecksum(reader);
    request.authToken = reader.readString();
    request.requestedSessionNonce = reader.readString();

    if (!reader.eof())
        throw std::runtime_error("handshake request contains trailing bytes");
    if (request.authToken.size() > AuthTokenMaxBytes)
        throw std::length_error("handshake auth token exceeds AuthTokenMaxBytes");

    return request;
}

Bytes serializeHandshakeAccept(const HandshakeAccept& accept)
{
    Bytes payload;
    ByteWriter writer(payload);
    writer.writePod(accept.assignedClientId);
    writer.writePod(accept.serverTickRate);
    writer.writePod(accept.snapshotRate);
    writer.writeString(accept.sessionNonce);
    writer.writePod(static_cast<std::uint8_t>(accept.resumedSession ? 1 : 0));
    return payload;
}

HandshakeAccept deserializeHandshakeAccept(const Bytes& payload)
{
    ByteReader reader(payload);
    HandshakeAccept accept;
    accept.assignedClientId = reader.readPod<ClientId>();
    accept.serverTickRate = reader.readPod<std::uint32_t>();
    accept.snapshotRate = reader.readPod<std::uint32_t>();
    accept.sessionNonce = reader.readString();
    accept.resumedSession = reader.readPod<std::uint8_t>() != 0;
    if (!reader.eof())
        throw std::runtime_error("handshake accept contains trailing bytes");
    return accept;
}

Bytes serializeHandshakeReject(const HandshakeReject& reject)
{
    return serializeRejectPayload(reject.reason, reject.message);
}

HandshakeReject deserializeHandshakeReject(const Bytes& payload)
{
    ByteReader reader(payload);
    HandshakeReject reject;
    reject.reason = static_cast<DisconnectReason>(reader.readPod<std::uint16_t>());
    reject.message = reader.readString();
    if (!reader.eof())
        throw std::runtime_error("handshake reject contains trailing bytes");
    return reject;
}

Bytes serializeDisconnectNotice(const DisconnectNotice& notice)
{
    return serializeRejectPayload(notice.reason, notice.diagnostic);
}

DisconnectNotice deserializeDisconnectNotice(const Bytes& payload)
{
    ByteReader reader(payload);
    DisconnectNotice notice;
    notice.reason = static_cast<DisconnectReason>(reader.readPod<std::uint16_t>());
    notice.diagnostic = reader.readString();
    if (!reader.eof())
        throw std::runtime_error("disconnect notice contains trailing bytes");
    return notice;
}

std::optional<HandshakeReject> validateHandshake(const HandshakePolicy& policy, const HandshakeRequest& request)
{
    if (request.protocolVersion != policy.protocolVersion)
        return HandshakeReject{ DisconnectReason::ProtocolMismatch, "protocol version mismatch" };

    if (request.buildId != policy.buildId)
        return HandshakeReject{ DisconnectReason::ProtocolMismatch, "build id mismatch" };

    if (request.assetChecksum != policy.assetChecksum || request.scriptChecksum != policy.scriptChecksum)
        return HandshakeReject{ DisconnectReason::AssetMismatch, "asset or script checksum mismatch" };

    if (!policy.acceptedAuthTokens.empty())
    {
        const auto tokenAccepted = std::find(policy.acceptedAuthTokens.begin(), policy.acceptedAuthTokens.end(),
            request.authToken);
        if (tokenAccepted == policy.acceptedAuthTokens.end())
            return HandshakeReject{ DisconnectReason::AuthFailed, "auth token rejected" };
    }

    return std::nullopt;
}

NetMessage makeHandshakeRequestMessage(const HandshakeRequest& request, Sequence sequence)
{
    return NetMessage{ MessageType::HandshakeRequest, Channel::ReliableOrdered, sequence, serializeHandshakeRequest(request) };
}

NetMessage makeHandshakeAcceptMessage(const HandshakeAccept& accept, Sequence sequence)
{
    return NetMessage{ MessageType::HandshakeAccept, Channel::ReliableOrdered, sequence, serializeHandshakeAccept(accept) };
}

NetMessage makeHandshakeRejectMessage(const HandshakeReject& reject, Sequence sequence)
{
    return NetMessage{ MessageType::HandshakeReject, Channel::ReliableOrdered, sequence, serializeHandshakeReject(reject) };
}

NetMessage makeDisconnectNoticeMessage(const DisconnectNotice& notice, Sequence sequence)
{
    return NetMessage{ MessageType::DisconnectNotice, Channel::ReliableOrdered, sequence,
        serializeDisconnectNotice(notice) };
}
} // namespace xrmp::net
