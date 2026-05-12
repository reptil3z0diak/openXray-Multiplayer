#include "Handshake.h"
#include "NetMessageCodec.h"

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
} // namespace

int main()
{
    HandshakeRequest request;
    request.protocolVersion = ProtocolVersion;
    request.buildId = 7;
    request.assetChecksum = checksum(0x11);
    request.scriptChecksum = checksum(0x22);
    request.authToken = "token";

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
    ok &= expect(decodedRequest.authToken == request.authToken, "auth token roundtrip");

    const HandshakePolicy accepted{ ProtocolVersion, 7, checksum(0x11), checksum(0x22), "token" };
    ok &= expect(!validateHandshake(accepted, decodedRequest).has_value(), "valid handshake accepted");

    const HandshakePolicy rejected{ ProtocolVersion, 7, checksum(0xFF), checksum(0x22), "token" };
    const auto rejection = validateHandshake(rejected, decodedRequest);
    ok &= expect(rejection.has_value(), "invalid checksum rejected");
    ok &= expect(rejection && rejection->reason == DisconnectReason::AssetMismatch, "reject reason asset mismatch");

    return ok ? 0 : 1;
}
