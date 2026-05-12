#include "SyncVar.h"

namespace xrmp::script
{
net::Bytes serializeSyncVarUpdate(const SyncVarUpdate& update)
{
    net::Bytes bytes;
    net::ByteWriter writer(bytes);
    writer.writeString(update.name);
    writer.writePod(static_cast<std::uint8_t>(update.type));
    writer.writePod(update.revision);
    writer.writeBytes(update.payload.data(), update.payload.size());
    return bytes;
}

SyncVarUpdate deserializeSyncVarUpdate(const net::Bytes& bytes)
{
    net::ByteReader reader(bytes);
    SyncVarUpdate update;
    update.name = reader.readString();
    update.type = static_cast<SyncVarType>(reader.readPod<std::uint8_t>());
    update.revision = reader.readPod<std::uint32_t>();
    update.payload = reader.remainingBytes();
    return update;
}
} // namespace xrmp::script
