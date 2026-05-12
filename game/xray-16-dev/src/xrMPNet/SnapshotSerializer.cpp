#include "SnapshotSerializer.h"

#include "BitStream.h"

#include <limits>
#include <stdexcept>

namespace xrmp::rep
{
namespace
{
constexpr std::uint32_t SnapshotWireVersion = 1;

void writeVec3Quantized(net::BitWriter& writer, const Vec3& value, const SnapshotQuantizationConfig& config)
{
    writer.writeQuantizedFloat(value.x, config.positionMin.x, config.positionMax.x, config.positionBits);
    writer.writeQuantizedFloat(value.y, config.positionMin.y, config.positionMax.y, config.positionBits);
    writer.writeQuantizedFloat(value.z, config.positionMin.z, config.positionMax.z, config.positionBits);
}

Vec3 readVec3Quantized(net::BitReader& reader, const SnapshotQuantizationConfig& config)
{
    Vec3 value;
    value.x = reader.readQuantizedFloat(config.positionMin.x, config.positionMax.x, config.positionBits);
    value.y = reader.readQuantizedFloat(config.positionMin.y, config.positionMax.y, config.positionBits);
    value.z = reader.readQuantizedFloat(config.positionMin.z, config.positionMax.z, config.positionBits);
    return value;
}

void writeRotation(net::BitWriter& writer, const Vec3& rotation, const SnapshotQuantizationConfig& config)
{
    writer.writeAngle(rotation.x, config.angleBits);
    writer.writeAngle(rotation.y, config.angleBits);
    writer.writeAngle(rotation.z, config.angleBits);
}

Vec3 readRotation(net::BitReader& reader, const SnapshotQuantizationConfig& config)
{
    Vec3 rotation;
    rotation.x = reader.readAngle(config.angleBits);
    rotation.y = reader.readAngle(config.angleBits);
    rotation.z = reader.readAngle(config.angleBits);
    return rotation;
}
} // namespace

net::Bytes SnapshotSerializer::serialize(const SnapshotFrame& frame, const SnapshotQuantizationConfig& config)
{
    if (frame.entities.size() > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("snapshot entity count exceeds u16 range");
    if (frame.removedEntities.size() > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("snapshot removed entity count exceeds u16 range");

    net::BitWriter writer;
    writer.writeBits(SnapshotWireVersion, 8);
    writer.writeBits(frame.sequence, 32);
    writer.writeBits(frame.serverTick, 32);
    writer.writeBits(frame.serverTimeMs, 32);
    writer.writeBits(static_cast<std::uint16_t>(frame.entities.size()), 16);
    writer.writeBits(static_cast<std::uint16_t>(frame.removedEntities.size()), 16);

    for (const EntitySnapshotState& entity : frame.entities)
    {
        writer.writeBits(entity.entityId, 64);
        writer.writeBits(entity.componentMask & 0x0F, 4);

        if ((entity.componentMask & toMask(RepComponentBit::Transform)) != 0)
        {
            if (!entity.transform)
                throw std::runtime_error("snapshot transform mask set without transform payload");

            writeVec3Quantized(writer, entity.transform->position, config);
            writeRotation(writer, entity.transform->rotation, config);
            writer.writeBool(entity.transform->teleported);
        }

        if ((entity.componentMask & toMask(RepComponentBit::Health)) != 0)
        {
            if (!entity.health)
                throw std::runtime_error("snapshot health mask set without health payload");

            writer.alignToByte();
            writer.writePod(entity.health->current);
            writer.writePod(entity.health->maximum);
            writer.writeBool(entity.health->alive);
        }

        if ((entity.componentMask & toMask(RepComponentBit::Animation)) != 0)
        {
            if (!entity.animation)
                throw std::runtime_error("snapshot animation mask set without animation payload");

            writer.writeString(entity.animation->motion);
            writer.alignToByte();
            writer.writePod(entity.animation->phase);
            writer.writeBool(entity.animation->looping);
        }

        if ((entity.componentMask & toMask(RepComponentBit::Inventory)) != 0)
        {
            if (!entity.inventory)
                throw std::runtime_error("snapshot inventory mask set without inventory payload");
            if (entity.inventory->items.size() > std::numeric_limits<std::uint16_t>::max())
                throw std::length_error("snapshot inventory item count exceeds u16 range");

            writer.alignToByte();
            writer.writePod(entity.inventory->activeItemId);
            writer.writeBits(static_cast<std::uint16_t>(entity.inventory->items.size()), 16);
            for (const InventoryItemRep& item : entity.inventory->items)
            {
                writer.writePod(item.itemId);
                writer.writePod(item.slot);
                writer.writePod(item.quantity);
                writer.writeString(item.section);
            }
        }
    }

    for (const NetEntityId entityId : frame.removedEntities)
        writer.writeBits(entityId, 64);

    return writer.takeBytes();
}

SnapshotFrame SnapshotSerializer::deserialize(const net::Bytes& bytes, const SnapshotQuantizationConfig& config)
{
    net::BitReader reader(bytes);
    const auto version = static_cast<std::uint8_t>(reader.readBits(8));
    if (version != SnapshotWireVersion)
        throw std::runtime_error("unsupported snapshot wire version");

    SnapshotFrame frame;
    frame.sequence = static_cast<std::uint32_t>(reader.readBits(32));
    frame.serverTick = static_cast<std::uint32_t>(reader.readBits(32));
    frame.serverTimeMs = static_cast<std::uint32_t>(reader.readBits(32));
    const auto entityCount = static_cast<std::uint16_t>(reader.readBits(16));
    const auto removedCount = static_cast<std::uint16_t>(reader.readBits(16));
    frame.entities.reserve(entityCount);
    frame.removedEntities.reserve(removedCount);

    for (std::uint16_t index = 0; index < entityCount; ++index)
    {
        EntitySnapshotState entity;
        entity.entityId = reader.readBits(64);
        entity.componentMask = reader.readBits(4);

        if ((entity.componentMask & toMask(RepComponentBit::Transform)) != 0)
        {
            TransformRep transform;
            transform.position = readVec3Quantized(reader, config);
            transform.rotation = readRotation(reader, config);
            transform.teleported = reader.readBool();
            entity.transform = transform;
        }

        if ((entity.componentMask & toMask(RepComponentBit::Health)) != 0)
        {
            reader.alignToByte();
            HealthRep health;
            health.current = reader.readPod<float>();
            health.maximum = reader.readPod<float>();
            health.alive = reader.readBool();
            entity.health = health;
        }

        if ((entity.componentMask & toMask(RepComponentBit::Animation)) != 0)
        {
            AnimationRep animation;
            animation.motion = reader.readString();
            reader.alignToByte();
            animation.phase = reader.readPod<float>();
            animation.looping = reader.readBool();
            entity.animation = animation;
        }

        if ((entity.componentMask & toMask(RepComponentBit::Inventory)) != 0)
        {
            reader.alignToByte();
            InventoryRep inventory;
            inventory.activeItemId = reader.readPod<NetEntityId>();
            const auto itemCount = static_cast<std::uint16_t>(reader.readBits(16));
            inventory.items.reserve(itemCount);
            for (std::uint16_t itemIndex = 0; itemIndex < itemCount; ++itemIndex)
            {
                InventoryItemRep item;
                item.itemId = reader.readPod<NetEntityId>();
                item.slot = reader.readPod<std::uint16_t>();
                item.quantity = reader.readPod<std::uint16_t>();
                item.section = reader.readString();
                inventory.items.push_back(std::move(item));
            }
            entity.inventory = std::move(inventory);
        }

        frame.entities.push_back(std::move(entity));
    }

    for (std::uint16_t index = 0; index < removedCount; ++index)
        frame.removedEntities.push_back(reader.readBits(64));

    return frame;
}
} // namespace xrmp::rep
