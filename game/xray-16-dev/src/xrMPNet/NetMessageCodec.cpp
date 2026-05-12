#include "NetMessageCodec.h"

#include "ByteStream.h"

#include <stdexcept>

namespace xrmp::net
{
namespace
{
constexpr std::uint32_t WireMagic = 0x504D5258; // XRMP
constexpr std::uint16_t WireVersion = 1;
}

Bytes serializeMessage(const NetMessage& message)
{
    if (message.payload.size() > MaxPayloadBytes)
        throw std::length_error("xrmp message payload exceeds MaxPayloadBytes");

    Bytes bytes;
    bytes.reserve(16 + message.payload.size());
    ByteWriter writer(bytes);
    writer.writePod(WireMagic);
    writer.writePod(WireVersion);
    writer.writePod(static_cast<std::uint16_t>(message.type));
    writer.writePod(static_cast<std::uint8_t>(message.channel));
    writer.writePod(message.sequence);
    writer.writePod(static_cast<std::uint32_t>(message.payload.size()));
    writer.writeBytes(message.payload.data(), message.payload.size());
    return bytes;
}

NetMessage deserializeMessage(const Bytes& bytes)
{
    ByteReader reader(bytes);
    const auto magic = reader.readPod<std::uint32_t>();
    if (magic != WireMagic)
        throw std::runtime_error("xrmp message has invalid wire magic");

    const auto version = reader.readPod<std::uint16_t>();
    if (version != WireVersion)
        throw std::runtime_error("xrmp message has unsupported wire version");

    NetMessage message;
    message.type = static_cast<MessageType>(reader.readPod<std::uint16_t>());
    message.channel = static_cast<Channel>(reader.readPod<std::uint8_t>());
    message.sequence = reader.readPod<Sequence>();

    const auto payloadSize = reader.readPod<std::uint32_t>();
    if (payloadSize > MaxPayloadBytes)
        throw std::length_error("xrmp message payload exceeds MaxPayloadBytes");

    message.payload = reader.readBytes(payloadSize);
    if (!reader.eof())
        throw std::runtime_error("xrmp message contains trailing bytes");

    return message;
}
} // namespace xrmp::net
